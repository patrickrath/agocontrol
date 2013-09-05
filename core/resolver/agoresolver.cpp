/*
     Copyright (C) 2012 Harald Klein <hari@vt100.at>

     This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License.
     This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
     of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

     See the GNU General Public License for more details.

     this is the core resolver component for ago control 
*/

#include <iostream>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#include <termios.h>
#include <malloc.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include <sstream>
#include <map>
#include <deque>

#include <uuid/uuid.h>

#include <qpid/messaging/Connection.h>
#include <qpid/messaging/Message.h>
#include <qpid/messaging/Receiver.h>
#include <qpid/messaging/Sender.h>
#include <qpid/messaging/Session.h>
#include <qpid/messaging/Address.h>

#include "../../shared/agoclient.h"
#include "../../version.h"

#include "schema.h"
#include "inventory.h"

using namespace std;
using namespace qpid::messaging;
using namespace qpid::types;
using namespace agocontrol;

AgoConnection *agoConnection;

Variant::Map inventory; // used to hold device registrations
Variant::Map schema;  
Variant::Map system; // holds system information

Inventory *inv;

bool emitNameEvent(const char *uuid, const char *eventType, const char *name) {
        Variant::Map content;
        content["name"] = name;
        content["uuid"] = uuid;
        return agoConnection->sendMessage(eventType, content);
}

bool emitFloorplanEvent(const char *uuid, const char *eventType, const char *floorplan, int x, int y) {
        Variant::Map content;
        content["uuid"] = uuid;
        content["floorplan"] = floorplan;
        content["x"] = x;
        content["y"] = y;
        return agoConnection->sendMessage(eventType, content);
}

// helper to determine last element
template <typename Iter>
Iter next(Iter iter)
{
	return ++iter;
}

string valuesToString(Variant::Map *values) {
	string result;
	for (Variant::Map::const_iterator it = values->begin(); it != values->end(); ++it) {
		result += it->second.asString();
		if ((it != values->end()) && (next(it) != values->end())) result += "/";	
	}
	return result;
}

void handleEvent(Variant::Map *device, string subject, Variant::Map *content) {
	Variant::Map *values;
	values = &(*device)["values"].asMap();
	if (subject == "event.device.statechanged") {// event.device.statechange
		(*values)["state"] = (*content)["level"];
		(*device)["state"]  = (*content)["level"];
		(*device)["state"].setEncoding("utf8");
		// (*device)["state"]  = valuesToString(values);
	} else if ((subject.find("event.environment.") != std::string::npos) && (subject.find("changed")!= std::string::npos)) {
		Variant::Map value;
		string quantity = subject;
		quantity.erase(quantity.begin(),quantity.begin()+18);
		quantity.erase(quantity.end()-7,quantity.end());

		value["unit"] = (*content)["unit"];
		value["level"] = (*content)["level"];
		(*values)[quantity] = value;
	}
}

qpid::types::Variant::Map commandHandler(qpid::types::Variant::Map content) {
	qpid::types::Variant::Map returnval;
	std::string internalid = content["internalid"].asString();
	Variant::Map reply;
	if (internalid == "agocontroller") {
		bool isHandled = false;
		if (content["command"] == "setroomname") {
			string uuid = content["uuid"];
			// if no uuid is provided, we need to generate one for a new room
			if (uuid == "") uuid = generateUuid();
			inv->setroomname(uuid, content["name"]);
			reply["uuid"] = uuid;
			reply["returncode"] = 0;
			isHandled = true;
			emitNameEvent(uuid.c_str(), "event.system.roomnamechanged", content["name"].asString().c_str());
		} else if (content["command"] == "setdeviceroom") {
			if ((content["uuid"].asString() != "") && (inv->setdeviceroom(content["uuid"], content["room"]) == 0)) {
				reply["returncode"] = 0;
				// update room in local device map
				Variant::Map *device;
				string room = inv->getdeviceroom(content["uuid"]);
				string uuid = content["uuid"];
				device = &inventory[uuid].asMap();
				(*device)["room"]= room;
			} else {
				reply["returncode"] = -1;
			}
			isHandled = true;
		} else if (content["command"] == "setdevicename") {
			if ((content["uuid"].asString() != "") && (inv->setdevicename(content["uuid"], content["name"]) == 0)) {
				reply["returncode"] = 0;
				// update name in local device map
				Variant::Map *device;
				string name = inv->getdevicename(content["uuid"]);
				string uuid = content["uuid"];
				device = &inventory[uuid].asMap();
				(*device)["name"]= name;
				emitNameEvent(content["uuid"].asString().c_str(), "event.system.devicenamechanged", content["name"].asString().c_str());
			} else {
				reply["returncode"] = -1;
			}
			isHandled = true;

		} else if (content["command"] == "deleteroom") {
			if (inv->deleteroom(content["uuid"]) == 0) {
				string uuid = content["uuid"].asString();
				emitNameEvent(uuid.c_str(), "event.system.roomdeleted", "");
				reply["returncode"] = 0;
			} else {
				reply["returncode"] = -1;
			}
			isHandled = true;
		} else if (content["command"] == "setfloorplanname") {
			string uuid = content["uuid"];
			// if no uuid is provided, we need to generate one for a new floorplan
			if (uuid == "") uuid = generateUuid();
			if (inv->setfloorplanname(uuid, content["name"]) == 0) {
				reply["uuid"] = uuid;
				reply["returncode"] = 0;
				emitNameEvent(content["uuid"].asString().c_str(), "event.system.floorplannamechanged", content["name"].asString().c_str());
			} else {
				reply["returncode"] = -1;
			}
			isHandled = true;
		} else if (content["command"] == "setdevicefloorplan") {
			if ((content["uuid"].asString() != "") && (inv->setdevicefloorplan(content["uuid"], content["floorplan"], content["x"], content["y"]) == 0)) {
				reply["returncode"] = 0;
				emitFloorplanEvent(content["uuid"].asString().c_str(), "event.system.floorplandevicechanged", content["floorplan"].asString().c_str(), content["x"], content["y"]);
			} else {
				reply["returncode"] = -1;
			}
			isHandled = true;

		} else if (content["command"] == "deletefloorplan") {
			if (inv->deletefloorplan(content["uuid"]) == 0) {
				reply["returncode"] = 0;
				emitNameEvent(content["uuid"].asString().c_str(), "event.system.floorplandeleted", "");
			} else {
				reply["returncode"] = -1;
			}
			isHandled = true;
		}
		if (isHandled) { 
			/* Message response;
			encode(reply, response);
			replyMessage(message.getReplyTo(), response);
			*/
		}
	} else {
		if (content["command"] == "inventory") {
			cout << "responding to inventory request" << std::endl;
			reply["inventory"] = inventory;
			reply["schema"] = schema;	
			reply["rooms"] = inv->getrooms();
			reply["floorplans"] = inv->getfloorplans();
			reply["system"] = system;
			reply["returncode"] = 0;
			/* Message response;
			encode(reply, response);
			replyMessage(message.getReplyTo(), response); */
		}
	}
	return reply;
}

void eventHandler(std::string subject, qpid::types::Variant::Map content) {
	if (subject == "event.device.announce") {
		string uuid = content["uuid"];
		if (uuid != "") {
			// clog << agocontrol::kLogDebug << "preparing device: uuid="  << uuid << std::endl;
			Variant::Map device;
			Variant::Map values;
			device["devicetype"]=content["devicetype"].asString();
			device["internalid"]=content["internalid"].asString();
			device["handled-by"]=content["handled-by"].asString();
			// clog << agocontrol::kLogDebug << "getting name from inventory" << endl;
			device["name"]=inv->getdevicename(content["uuid"].asString());
			device["name"].setEncoding("utf8");
			// clog << agocontrol::kLogDebug << "getting room from inventory" << endl;
			device["room"]=inv->getdeviceroom(content["uuid"].asString()); 
			device["room"].setEncoding("utf8");
			device["state"]="0";
			device["state"].setEncoding("utf8");
			device["values"]=values;
			cout << "adding device: uuid="  << uuid  << " type: " << device["devicetype"].asString() << std::endl;
			// clog << agocontrol::kLogDebug << "adding device: uuid="  << uuid  << " type: " << device["devicetype"].asString() << std::endl;
			inventory[uuid] = device;
		}
	} else if (subject == "event.device.remove") {
		string uuid = content["uuid"];
		if (uuid != "") {
			// clog << agocontrol::kLogDebug << "removing device: uuid="  << uuid  << std::endl;
			Variant::Map::iterator it = inventory.find(uuid);
			if (it != inventory.end()) {
				inventory.erase(it);
			}
		}
	} else {
		if (content["uuid"].asString() != "") {
			string uuid = content["uuid"];
			// see if we have that device in the inventory already, if yes handle the event
			if (inventory.find(uuid) != inventory.end()) handleEvent(&inventory[uuid].asMap(), subject, &content);
		}

	}
}

int main(int argc, char **argv) {
//	clog.rdbuf(new agocontrol::Log("agoresolver", LOG_LOCAL0));

	agoConnection = new AgoConnection("resolver");
	agoConnection->addHandler(commandHandler);
	agoConnection->addEventHandler(eventHandler);
	agoConnection->setFilter(false);

	string schemafile;

//	clog << agocontrol::kLogNotice << "starting up" << std::endl;

	schemafile=getConfigOption("system", "schema", "/etc/opt/agocontrol/schema.yaml");

	system["uuid"] = getConfigOption("system", "uuid", "00000000-0000-0000-000000000000");
	system["version"] = AGOCONTROL_VERSION;

//	clog << agocontrol::kLogDebug << "parsing schema file" << std::endl;
	schema = parseSchema(schemafile.c_str());

//	clog << agocontrol::kLogDebug << "reading inventory" << std::endl;
	inv = new Inventory("/etc/opt/agocontrol/inventory.db");

	agoConnection->addDevice("agocontroller","agocontroller");
	
	// discover devices
//	clog << agocontrol::kLogDebug << "discovering devices" << std::endl;
	Variant::Map discovercmd;
	discovercmd["command"] = "discover";
	agoConnection->sendMessage("",discovercmd);
	agoConnection->run();	
}

