/*
 * Copyright 2013 Arx Libertatis Team (see the AUTHORS file)
 *
 * This file is part of Arx Libertatis.
 *
 * Arx Libertatis is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Arx Libertatis is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Arx Libertatis.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "gui/DebugHud.h"

#include <cstdio>
#include <string>
#include <iomanip>
#include <deque>

#include <boost/format.hpp>

#include "core/Core.h"
#include "core/Application.h"
#include "core/Version.h"
#include "core/GameTime.h"

#include "game/Player.h"

#include "math/Types.h"

#include "graphics/particle/ParticleEffects.h"
#include "graphics/font/Font.h"
#include "gui/Text.h"
#include "gui/Interface.h"

#include "ai/PathFinderManager.h"
#include "script/ScriptEvent.h"
#include "scene/Interactive.h"
#include "game/EntityManager.h"
#include "game/NPC.h"

#include "graphics/Renderer.h"
#include "graphics/DrawLine.h"

#include "window/RenderWindow.h"

template <typename T>
struct FlagName {
	T flag;
	const char * name;
};

const FlagName<EntityFlags> EntityFlagNames[] = {
	{IO_UNDERWATER          , "UNDERWATER"},
	{IO_FREEZESCRIPT        , "FREEZESCRIPT"},
	{IO_ITEM                , "ITEM"},
	{IO_NPC                 , "NPC"},
	{IO_FIX                 , "FIX"},
	{IO_NOSHADOW            , "NOSHADOW"},
	{IO_CAMERA              , "CAMERA"},
	{IO_MARKER              , "MARKER"},
	{IO_ICONIC              , "ICONIC"},
	{IO_NO_COLLISIONS       , "NO_COLLISIONS"},
	{IO_GOLD                , "GOLD"},
	{IO_INVULNERABILITY     , "INVULNERABILITY"},
	{IO_NO_PHYSICS_INTERPOL , "NO_PHYSICS_INTERPOL"},
	{IO_HIT                 , "HIT"},
	{IO_PHYSICAL_OFF        , "PHYSICAL_OFF"},
	{IO_MOVABLE             , "MOVABLE"},
	{IO_UNIQUE              , "UNIQUE"},
	{IO_SHOP                , "SHOP"},
	{IO_BLACKSMITH          , "BLACKSMITH"},
	{IO_NOSAVE              , "NOSAVE"},
	{IO_FORCEDRAW           , "FORCEDRAW"},
	{IO_FIELD               , "FIELD"},
	{IO_BUMP                , "BUMP"},
	{IO_ANGULAR             , "ANGULAR"},
	{IO_BODY_CHUNK          , "BODY_CHUNK"},
	{IO_ZMAP                , "ZMAP"},
	{IO_INVERTED            , "INVERTED"},
	{IO_JUST_COLLIDE        , "JUST_COLLIDE"},
	{IO_FIERY               , "FIERY"},
	{IO_NO_NPC_COLLIDE      , "NO_NPC_COLLIDE"},
	{IO_CAN_COMBINE         , "CAN_COMBINE"}
};

const FlagName<Behaviour> BehaviourFlagNames[] = {
	{BEHAVIOUR_NONE          , "NONE"},
	{BEHAVIOUR_FRIENDLY      , "FRIENDLY"},
	{BEHAVIOUR_MOVE_TO       , "MOVE_TO"},
	{BEHAVIOUR_WANDER_AROUND , "WANDER_AROUND"},
	{BEHAVIOUR_FLEE          , "FLEE"},
	{BEHAVIOUR_HIDE          , "HIDE"},
	{BEHAVIOUR_LOOK_FOR      , "LOOK_FOR"},
	{BEHAVIOUR_SNEAK         , "SNEAK"},
	{BEHAVIOUR_FIGHT         , "FIGHT"},
	{BEHAVIOUR_DISTANT       , "DISTANT"},
	{BEHAVIOUR_MAGIC         , "MAGIC"},
	{BEHAVIOUR_GUARD         , "GUARD"},
	{BEHAVIOUR_GO_HOME       , "GO_HOME"},
	{BEHAVIOUR_LOOK_AROUND   , "LOOK_AROUND"},
	{BEHAVIOUR_STARE_AT      , "STARE_AT"}
};

template <typename T, size_t N>
std::string flagNames(const FlagName<T> (&names)[N], const T flags) {
	std::stringstream ss;
	for(size_t i = 0; i < N; i++) {
		if(names[i].flag & flags) {
			ss << names[i].name << " ";
		}
	}
	return ss.str();
}

class DebugBox {
public:
	DebugBox(const Vec2i & pos, const std::string & title)
		: m_pos(pos)
		, m_title(title)
		, m_maxKeyLen(0)
	{}
	
	void add(std::string key, const std::string value) {
		m_maxKeyLen = std::max(m_maxKeyLen, key.length());
		m_elements.push_back(std::pair<std::string, std::string>(key, value));
	}
	
	void add(std::string key, const long value) {
		m_maxKeyLen = std::max(m_maxKeyLen, key.length());
		std::string valueStr = boost::str(boost::format("%ld") % value);
		m_elements.push_back(std::pair<std::string, std::string>(key, valueStr));
	}
	
	void add(std::string key, const double value) {
		m_maxKeyLen = std::max(m_maxKeyLen, key.length());
		std::string valueStr = boost::str(boost::format("%4.2f") % value);
		m_elements.push_back(std::pair<std::string, std::string>(key, valueStr));
	}
	
	void add(std::string key, const Vec2i value) {
		m_maxKeyLen = std::max(m_maxKeyLen, key.length());
		std::string valueStr = boost::str(boost::format("(%d, %d)") % value.x % value.y);
		m_elements.push_back(std::pair<std::string, std::string>(key, valueStr));
	}
	
	void add(std::string key, const Vec3f value) {
		m_maxKeyLen = std::max(m_maxKeyLen, key.length());
		std::string valueStr = boost::str(boost::format("%4.2f %4.2f %4.2f") % value.x % value.y % value.z);
		m_elements.push_back(std::pair<std::string, std::string>(key, valueStr));
	}
	
	void add(std::string key, const Anglef value) {
		m_maxKeyLen = std::max(m_maxKeyLen, key.length());
		std::string valueStr = boost::str(boost::format("%4.2f %4.2f %4.2f") % value.getYaw() % value.getPitch() % value.getRoll());
		m_elements.push_back(std::pair<std::string, std::string>(key, valueStr));
	}
	
	void add(std::string key, const ResourcePool value) {
		m_maxKeyLen = std::max(m_maxKeyLen, key.length());
		std::string valueStr = boost::str(boost::format("%4.2f/%4.2f") % value.current % value.max);
		m_elements.push_back(std::pair<std::string, std::string>(key, valueStr));
	}
	
	void print() {
		int lineHeight = hFontDebug->getLineHeight();
		Vec2i lineOffset = m_pos;
		
		hFontDebug->draw(lineOffset, std::string("╭─ ") + m_title, Color::white);
		lineOffset.y += lineHeight;
		
		std::vector<std::pair<std::string, std::string> >::const_iterator itr;
		for(itr = m_elements.begin(); itr != m_elements.end(); ++itr) {
			std::stringstream out;
			out << "│ " << std::left << std::setw(m_maxKeyLen) << std::setfill(' ') << itr->first << " " << itr->second;
			hFontDebug->draw(lineOffset, out.str(), Color::white);
			lineOffset.y += lineHeight;
		}
		
		hFontDebug->draw(lineOffset, std::string("╰─ "), Color::white);
		lineOffset.y += lineHeight;
		
		m_size = lineOffset;
	}
	
	Vec2i size() {
		return m_size;
	}
	
private:
	Vec2i m_pos;
	std::string m_title;
	size_t m_maxKeyLen;
	Vec2i m_size;
	
	
	std::vector<std::pair<std::string, std::string> > m_elements;
};

std::string LAST_FAILED_SEQUENCE = "none";
extern float CURRENT_PLAYER_COLOR;
EntityHandle LastSelectedIONum = EntityHandle::Invalid;

void ShowInfoText() {
	
	DebugBox frameInfo = DebugBox(Vec2i(10, 10), "FrameInfo");
	frameInfo.add("Prims", EERIEDrawnPolys);
	frameInfo.add("Particles", getParticleCount());
	frameInfo.add("TIME", static_cast<long>((unsigned long)(arxtime) / 1000));
	frameInfo.print();
	
	DebugBox playerBox = DebugBox(Vec2i(10, frameInfo.size().y + 5), "Player");
	playerBox.add("Position", player.pos);
	playerBox.add("AnchorPos", player.pos - Mscenepos);
	playerBox.add("Rotation", player.angle);
	playerBox.add("Velocity", player.physics.velocity);
	
	EERIEPOLY * ep = CheckInPoly(player.pos);
	float truePolyY = -666.66f;
	if(ep) {
		float tempY = 0.f;
		if(GetTruePolyY(ep, player.pos, &tempY)) {
			truePolyY = tempY;
		}
	}
	
	ep = CheckInPoly(player.pos + Vec3f(0.f, -10.f, 0.f));
	float slope = 0.f;
	if(ep)
		slope = ep->norm.y;
	
	long zap = IsAnyPolyThere(player.pos.x,player.pos.z);
	
	playerBox.add("Ground Slope", slope);
	playerBox.add("Ground truePolyY", truePolyY);
	playerBox.add("Ground POLY", zap);
	playerBox.add("Color", CURRENT_PLAYER_COLOR);
	playerBox.add("Stealth", GetPlayerStealth());
	
	playerBox.add("Jump", player.jumplastposition);
	playerBox.add("OFFGRND", (!player.onfirmground ? "OFFGRND" : ""));
	
	playerBox.add("Life", player.lifePool);
	playerBox.add("Mana", player.manaPool);
	playerBox.add("Poisoned", player.poison);
	playerBox.add("Hunger", player.hunger);
	playerBox.add("Magic", static_cast<long>(player.doingmagic));
	playerBox.print();
	
	DebugBox miscBox = DebugBox(Vec2i(10, playerBox.size().y + 5), "Misc");
	miscBox.add("Arx version", arx_version);
	miscBox.add("Level", LastLoadedScene.string().c_str());
	miscBox.add("Spell failed seq", LAST_FAILED_SEQUENCE.c_str());
	miscBox.add("Camera focal", ACTIVECAM->focal);
	miscBox.add("Cinema", CINEMA_DECAL);
	miscBox.add("Mouse", Vec2i(DANAEMouse));
	miscBox.add("Pathfind queue", EERIE_PATHFINDER_Get_Queued_Number());
	miscBox.add("Pathfind status", (PATHFINDER_WORKING ? "Working" : "Idled"));
	miscBox.print();
	
	{
	struct ScriptDebugReport {
		std::string entityName;
		long events;
		long sends;
		
		ScriptDebugReport()
			: entityName("")
			, events(0)
			, sends(0)
		{}
	};
	
	ScriptDebugReport maxEvents;
	Entity * io = ARX_SCRIPT_Get_IO_Max_Events();
	if(io) {
		maxEvents.entityName = io->idString();
		maxEvents.events = io->stat_count;
	}
	
	ScriptDebugReport maxSender;
	io = ARX_SCRIPT_Get_IO_Max_Events_Sent();
	if(io) {
		maxSender.entityName = io->idString();
		maxSender.sends = io->stat_sent;
	}
	
	DebugBox scriptBox = DebugBox(Vec2i(10, miscBox.size().y + 5), "Script");
	scriptBox.add("Events", ScriptEvent::totalCount);
	scriptBox.add("Timers", ARX_SCRIPT_CountTimers());
	scriptBox.add("Max events", maxEvents.entityName);
	scriptBox.add("Max events#", maxEvents.events);
	scriptBox.add("Max sender", maxSender.entityName);
	scriptBox.add("Max sender#", maxSender.sends);
	scriptBox.print();
	}
	
	if(ValidIONum(LastSelectedIONum)) {
		Entity * io = entities[LastSelectedIONum];

		if(io) {
			DebugBox entityBox = DebugBox(Vec2i(500, 10), "Entity " + io->idString());
			entityBox.add("Pos", io->pos);
			entityBox.add("Angle", io->angle);
			entityBox.add("Room", static_cast<long>(io->room));
			entityBox.add("Move", io->move);
			entityBox.add("Flags", flagNames(EntityFlagNames, io->ioflags));
			entityBox.print();
			
			if(io->ioflags & IO_NPC) {
				IO_NPCDATA * npcData = io->_npcdata;
				
				DebugBox npcBox = DebugBox(Vec2i(500, entityBox.size().y + 5), "NPC");
				npcBox.add("Life", npcData->lifePool);
				npcBox.add("Mana", npcData->manaPool);
				npcBox.add("Poisoned", npcData->poisonned);
				npcBox.add("ArmorClass", ARX_INTERACTIVE_GetArmorClass(io));
				npcBox.add("Absorb", npcData->absorb);
				
				npcBox.add("Moveproblem", npcData->moveproblem);
				npcBox.add("Pathfind listpos", static_cast<long>(npcData->pathfind.listpos));
				npcBox.add("Pathfind listnb", npcData->pathfind.listnb);
				npcBox.add("Pathfind targ", npcData->pathfind.truetarget);
				npcBox.add("Behavior", flagNames(BehaviourFlagNames, npcData->behavior));
				
				// TODO should those really be flags ?
				PathfindFlags pflag = io->_npcdata->pathfind.flags;
				std::string pflags;
				if(pflag & PATHFIND_ALWAYS)    pflags += "ALWAYS ";
				if(pflag & PATHFIND_ONCE)      pflags += "ONCE ";
				if(pflag & PATHFIND_NO_UPDATE) pflags += "NO_UPDATE ";
				npcBox.add("Pathfind flgs", pflags);
				
				npcBox.print();
			}

			if(io->ioflags & (IO_FIX | IO_ITEM)) {
				DebugBox itemBox = DebugBox(Vec2i(500, entityBox.size().y + 5), "Item");
				
				itemBox.add("Durability", io->durability);
				itemBox.add("Durability max", io->max_durability);
				itemBox.add("Poisonous", static_cast<long>(io->poisonous));
				itemBox.add("Poisonous count", static_cast<long>(io->poisonous_count));
				itemBox.print();
			}
		}
	}
	
	ARX_SCRIPT_Init_Event_Stats();
}

void ShowFPS() {
	std::ostringstream oss;
	oss << std::fixed << std::setprecision(2) << FPS << " FPS";
	hFontDebug->draw(Vec2i(10, 10), oss.str(), Color::white);
}

void ShowDebugToggles() {
	for(size_t i = 0; i < ARRAY_SIZE(g_debugToggles); i++) {
		std::stringstream textStream;
		textStream << "Toggle " << i << ": " << (g_debugToggles[i] ? "true" : "false");
		hFontDebug->draw(0.f, i * hFontDebug->getLineHeight(), textStream.str(), Color::white);
	}
	
	for(size_t i = 0; i < ARRAY_SIZE(g_debugTriggersTime); i++) {
		u32 time = g_debugTriggersTime[i];
		float factor = platform::getElapsedMs(time) / g_debugTriggersDecayDuration;
		factor = glm::clamp(factor, 0.f, 1.f);
		
		Color3f color = Color3f(1.f, factor, factor);
		
		std::stringstream textStream;
		textStream << "Trigger " << i;
		hFontDebug->draw(150.f, i * hFontDebug->getLineHeight(), textStream.str(), color.to<u8>());
	}
}

void ShowFpsGraph() {

	GRenderer->ResetTexture(0);

	static std::deque<float> lastFPSArray;
	lastFPSArray.push_front(1000 / arxtime.get_frame_delay());

	Vec2i windowSize = mainApp->getWindow()->getSize();
	if(lastFPSArray.size() == size_t(windowSize.x))
	{
		lastFPSArray.pop_back();
	}

	float avg = 0;
	float worst = lastFPSArray[0];

	std::vector<TexturedVertex> vertices;
	vertices.resize(lastFPSArray.size());

	const float SCALE_Y = 2.0f;

	for(size_t i = 0; i < lastFPSArray.size(); ++i)
	{
		float time = lastFPSArray[i];

		avg += lastFPSArray[i];
		worst = std::min(worst, lastFPSArray[i]);

		vertices[i].color = Color(255, 255, 255, 255).toRGBA();
		vertices[i].p.x = i;
		vertices[i].p.y = windowSize.y - (time * SCALE_Y);
		vertices[i].p.z = 1.0f;
		vertices[i].rhw = 1.0f;
	}
	avg /= lastFPSArray.size();

	EERIEDRAWPRIM(Renderer::LineStrip, &vertices[0], vertices.size());

	Color avgColor = Color::blue * 0.5f + Color::white * 0.5f;
	float avgPos = windowSize.y - (avg * SCALE_Y);
	drawLine2D(0, avgPos,  windowSize.x, avgPos, 1.0f, Color::blue);

	Color worstColor = Color::red * 0.5f + Color::white * 0.5f;
	float worstPos = windowSize.y - (worst * SCALE_Y);
	drawLine2D(0, worstPos,  windowSize.x, worstPos, 1.0f, Color::red);

	Font * font = hFontDebug;
	float lineOffset = font->getLineHeight() + 2;

	std::string labels[3] = { "Average: ", "Worst: ", "Current: " };
	Color colors[3] = { avgColor, worstColor, Color::white };
	float values[3] = { avg, worst, lastFPSArray[0] };

	std::string texts[3];
	float widths[3];
	static float labelWidth = 0.f;
	static float valueWidth = 0.f;
	for(size_t i = 0; i < 3; i++) {
		// Format value
		std::ostringstream oss;
		oss << std::fixed << std::setprecision(2) << values[i] << " FPS";
		texts[i] = oss.str();
		// Calculate widths (could be done more efficiently for monospace fonts...)
		labelWidth = std::max(labelWidth, float(font->getTextSize(labels[i]).x));
		widths[i] = font->getTextSize(texts[i]).x;
		valueWidth = std::max(valueWidth, widths[i]);
	}

	float x = 10;
	float y = 10;
	float xend = x + labelWidth + 10 + valueWidth;
	for(size_t i = 0; i < 3; i++) {
		font->draw(Vec2i(x, y), labels[i], Color::gray(0.8f));
		font->draw(Vec2i(xend - widths[i], y), texts[i], colors[i]);
		y += lineOffset;
	}

}
