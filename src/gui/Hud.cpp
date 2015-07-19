/*
 * Copyright 2014 Arx Libertatis Team (see the AUTHORS file)
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

#include "gui/Hud.h"

#include <iomanip>
#include <sstream>

#include <boost/foreach.hpp>

#include "core/Application.h"
#include "core/ArxGame.h"
#include "core/Config.h"
#include "core/Core.h"
#include "core/GameTime.h"
#include "core/Localisation.h"

#include "game/Entity.h"
#include "game/EntityManager.h"
#include "game/Equipment.h"
#include "game/Inventory.h"
#include "game/Item.h"
#include "game/Player.h"
#include "game/Spells.h"

#include "graphics/Draw.h"
#include "graphics/Renderer.h"
#include "graphics/particle/ParticleEffects.h"
#include "graphics/data/TextureContainer.h"

#include "gui/Cursor.h"
#include "gui/Interface.h"
#include "gui/Speech.h"
#include "gui/book/Book.h"
#include "gui/hud/HudCommon.h"
#include "gui/hud/PlayerInventory.h"
#include "gui/hud/SecondaryInventory.h"

#include "input/Input.h"

#include "scene/GameSound.h"
#include "scene/Interactive.h"

extern float GLOBAL_SLOWDOWN;
extern float InventoryDir;
extern bool WILLRETURNTOFREELOOK;

bool bIsAiming = false;

/*!
 * \brief the hit strength diamond shown at the bottom of the UI.
 */
class HitStrengthGauge : public HudItem{
private:
	TextureContainer * m_emptyTex;
	TextureContainer * m_fullTex;
	TextureContainer * m_hitTex;
	
	Vec2f m_size;
	Vec2f m_hitSize;
	
	Rectf m_hitRect;
	
	float m_intensity;
	bool bHitFlash;
	unsigned long ulHitFlash;
	float m_flashIntensity;
	
public:
	HitStrengthGauge()
		: m_emptyTex(NULL)
		, m_fullTex(NULL)
		, m_hitTex(NULL)
		, m_intensity(0.f)
		, bHitFlash(false)
		, ulHitFlash(0)
		, m_flashIntensity(0.f)
	{}
	
	void init() {
		m_emptyTex = TextureContainer::LoadUI("graph/interface/bars/aim_empty");
		m_fullTex = TextureContainer::LoadUI("graph/interface/bars/aim_maxi");
		m_hitTex = TextureContainer::LoadUI("graph/interface/bars/flash_gauge");
		arx_assert(m_emptyTex);
		arx_assert(m_fullTex);
		arx_assert(m_hitTex);
		
		m_size = Vec2f(122.f, 70.f);
		m_hitSize = Vec2f(172.f, 130.f);
	}
	
	void requestFlash(float flashIntensity) {
		bHitFlash = true;
		ulHitFlash = 0;
		m_flashIntensity = flashIntensity;
	}
	
	void updateRect(const Rectf & parent) {
		m_rect = createChild(parent, Anchor_BottomCenter, m_size * m_scale, Anchor_BottomCenter);
		m_rect.move(0.f, -2.f);
		
		m_hitRect = createChild(m_rect, Anchor_Center, m_hitSize * m_scale, Anchor_Center);
	}
	
	void update() {
		
		if(AimTime == 0) {
			m_intensity = 0.2f;
		} else {
			float j;
			if(BOW_FOCAL) {
				j=(float)(BOW_FOCAL)/710.f;
			} else {
				float at=float(arxtime)-(float)AimTime;
				
				//TODO global
				bIsAiming = at > 0.f;
				
				at=at*(1.f+(1.f-GLOBAL_SLOWDOWN));
				float aim = static_cast<float>(player.Full_AimTime);
				j=at/aim;
			}
			m_intensity = glm::clamp(j, 0.2f, 1.f);
		}
		
		if(bHitFlash) {
			float fCalc = ulHitFlash + Original_framedelay;
			ulHitFlash = checked_range_cast<unsigned long>(fCalc);
			if(ulHitFlash >= 500) {
				bHitFlash = false;
				ulHitFlash = 0;
			}
		}
	}
	
	void draw() {
		GRenderer->SetBlendFunc(Renderer::BlendOne, Renderer::BlendOne);
		GRenderer->SetRenderState(Renderer::AlphaBlending, true);
		EERIEDrawBitmap(m_rect, 0.0001f, m_fullTex, Color3f::gray(m_intensity).to<u8>());
		
		GRenderer->SetRenderState(Renderer::AlphaBlending, false);
		EERIEDrawBitmap(m_rect, 0.0001f, m_emptyTex, Color::white);
		
		if(bHitFlash && player.m_skillFull.etheralLink >= 40) {
			
			float j = 1.0f - m_flashIntensity;
			Color col = (j < 0.5f) ? Color3f(j*2.0f, 1, 0).to<u8>() : Color3f(1, m_flashIntensity, 0).to<u8>();
			
			GRenderer->SetBlendFunc(Renderer::BlendOne, Renderer::BlendOne);
			GRenderer->SetRenderState(Renderer::AlphaBlending, true);
			EERIEDrawBitmap(m_hitRect, 0.0001f, m_hitTex, col);
			GRenderer->SetRenderState(Renderer::AlphaBlending, false);
		}
	}
};
HitStrengthGauge hitStrengthGauge = HitStrengthGauge();

void hitStrengthGaugeRequestFlash(float flashIntensity) {
	hitStrengthGauge.requestFlash(flashIntensity);
}




static void DrawItemPrice() {
	
	Entity *temp = SecondaryInventory->io;
	if(temp->ioflags & IO_SHOP) {
		Vec2f pos = Vec2f(DANAEMouse);
		pos += Vec2f(0, -10);
		
		if(g_secondaryInventoryHud.containsPos(DANAEMouse)) {
			long amount=ARX_INTERACTIVE_GetPrice(FlyingOverIO,temp);
			// achat
			float famount = amount - amount * player.m_skillFull.intuition * 0.005f;
			// check should always be OK because amount is supposed positive
			amount = checked_range_cast<long>(famount);

			Color color = (amount <= player.gold) ? Color::green : Color::red;
			
			ARX_INTERFACE_DrawNumber(pos, amount, 6, color, 1.f);
		} else if(g_playerInventoryHud.containsPos(DANAEMouse)) {
			long amount = static_cast<long>( ARX_INTERACTIVE_GetPrice( FlyingOverIO, temp ) / 3.0f );
			// achat
			float famount = amount + amount * player.m_skillFull.intuition * 0.005f;
			// check should always be OK because amount is supposed positive
			amount = checked_range_cast<long>( famount );

			if(amount) {
				Color color = Color::red;
				
				if(temp->shop_category.empty() ||
				   FlyingOverIO->groups.find(temp->shop_category) != FlyingOverIO->groups.end()) {

					color = Color::green;
				}
				ARX_INTERFACE_DrawNumber(pos, amount, 6, color, 1.f);
			}
		}
	}
}


extern TextureContainer * healing;

class BookIconGui : public HudIconBase {
private:
	
	void MakeBookFX(const Vec3f & pos) {
		
		for(long i = 0; i < 12; i++) {
			
			MagFX(pos);
		}
		
		for(int i = 0; i < 5; i++) {
			
			PARTICLE_DEF * pd = createParticle();
			if(!pd) {
				break;
			}
			
			pd->ov = pos - Vec3f(i * 2, i * 2, 0.f);
			pd->move = Vec3f(i * -0.5f, i * -0.5f, 0.f);
			pd->scale = Vec3f(i * 10, i * 10, 0.f);
			pd->tolive = Random::get(1200, 1600);
			pd->tc = m_tex;
			pd->rgb = Color3f(1.f - i * 0.1f, i * 0.1f, 0.5f - i * 0.1f);
			pd->siz = 32.f + i * 4.f;
			pd->is2D = true;
		}
		
		NewSpell = 1;
	}
	
	Vec2f m_size;
	
	unsigned long ulBookHaloTime;
	
public:
	BookIconGui()
		: HudIconBase()
		, m_size(Vec2f(32, 32))
		, ulBookHaloTime(0)
	{}
	
	void init() {
		m_tex = TextureContainer::LoadUI("graph/interface/icons/book");
		arx_assert(m_tex);
		
		m_size = Vec2f(32, 32);
		
		m_haloColor = Color3f(0.2f, 0.4f, 0.8f);
		
		m_haloActive = false;
		ulBookHaloTime = 0;
	}
	
	void requestHalo() {
		m_haloActive = true;
		ulBookHaloTime = 0;
	}
	
	void requestFX() {
		MakeBookFX(Vec3f(Vec2f(g_size.bottomRight()) + Vec2f(-35, -148), 0.00001f));
	}
	
	void update(const Rectf & parent) {
		
		m_rect = createChild(parent, Anchor_TopRight, m_size * m_scale, Anchor_BottomRight);
		
		if(m_haloActive) {
			float fCalc = ulBookHaloTime + Original_framedelay;
			ulBookHaloTime = checked_range_cast<unsigned long>(fCalc);
			if(ulBookHaloTime >= 3000) { // ms
				m_haloActive = false;
			}
		}
	}
	
	void updateInput() {
		m_isSelected = m_rect.contains(Vec2f(DANAEMouse));
		
		if(m_isSelected) {
			SpecialCursor = CURSOR_INTERACTION_ON;

			if(eeMouseDown1()) {
				ARX_INTERFACE_BookToggle();
			}
			return;
		}
	}
};

static BookIconGui bookIconGui;

void bookIconGuiRequestHalo() {
	bookIconGui.requestHalo();
}

void bookIconGuiRequestFX() {
	bookIconGui.requestFX();
}



class BackpackIconGui : public HudIconBase {
private:
	
public:
	void init() {
		m_tex = TextureContainer::LoadUI("graph/interface/icons/backpack");
		arx_assert(m_tex);
	}

	void update(const Rectf & parent) {
		
		m_rect = createChild(parent, Anchor_TopRight, Vec2f(32, 32) * m_scale, Anchor_BottomRight);
	}
	
	void updateInput() {
		{
		static float flDelay=0;
		
		// Check for backpack Icon
		if(m_rect.contains(Vec2f(DANAEMouse))) {
			if(eeMouseUp1() && playerInventory.insert(DRAGINTER)) {
				ARX_SOUND_PlayInterface(SND_INVSTD);
				Set_DragInter(NULL);
			}
		}
		
		if(m_rect.contains(Vec2f(DANAEMouse)) || flDelay) {
			eMouseState = MOUSE_IN_INVENTORY_ICON;
			SpecialCursor = CURSOR_INTERACTION_ON;
			
			
			if(eeMouseDoubleClick1()) {
				ARX_SOUND_PlayInterface(SND_BACKPACK, 0.9F + 0.2F * rnd());

				playerInventory.optimize();

				flDelay = 0;
			} else if(eeMouseDown1() || flDelay) {
				if(!flDelay) {
					flDelay = arxtime.get_updated();
					return;
				} else {
					if(arxtime.get_updated() - flDelay < 300) {
						return;
					} else {
						flDelay = 0;
					}
				}

				if(player.Interface & INTER_INVENTORYALL) {
					ARX_SOUND_PlayInterface(SND_BACKPACK, 0.9F + 0.2F * rnd());
					bInventoryClosing = true;
				} else {
					bInverseInventory=!bInverseInventory;
					lOldTruePlayerMouseLook=TRUE_PLAYER_MOUSELOOK_ON;
				}
			} else if(eeMouseDown2()) {
				ARX_INTERFACE_BookClose();
				ARX_INVENTORY_OpenClose(NULL);

				if(player.Interface & INTER_INVENTORYALL) {
					bInventoryClosing = true;
				} else {
					if(player.Interface & INTER_INVENTORY) {
						ARX_SOUND_PlayInterface(SND_BACKPACK, 0.9F + 0.2F * rnd());
						bInventoryClosing = true;
						bInventorySwitch = true;
					} else {
						ARX_SOUND_PlayInterface(SND_BACKPACK, 0.9F + 0.2F * rnd());
						player.Interface |= INTER_INVENTORYALL;

						float fInventoryY = INTERFACE_RATIO(121.f) * (player.bag);
						InventoryY = checked_range_cast<long>(fInventoryY);

						ARX_INTERFACE_NoteClose();

						if(TRUE_PLAYER_MOUSELOOK_ON) {
							WILLRETURNTOFREELOOK = true;
						}
					}
				}
				
				TRUE_PLAYER_MOUSELOOK_ON = false;
			}

			if(DRAGINTER == NULL)
				return;
		}
		}
	}
	
	void draw() {
		m_isSelected = eMouseState == MOUSE_IN_INVENTORY_ICON;
		HudIconBase::draw();
	}
};

static BackpackIconGui backpackIconGui;


class StealIconGui : public HudIconBase {
private:
	Vec2f m_size;
	Vec2f m_pos;
	
public:
	void init() {
		m_tex = TextureContainer::LoadUI("graph/interface/icons/steal");
		arx_assert(m_tex);
		
		m_size = Vec2f(32, 32);
	}
	
	void updateRect(const Rectf & parent) {
		
		m_rect = createChild(parent, Anchor_TopLeft, m_size * m_scale, Anchor_BottomLeft);
	}
	
	void updateInput() {
		
		// steal
		if(player.Interface & INTER_STEAL) {
			if(m_rect.contains(Vec2f(DANAEMouse))) {
				eMouseState=MOUSE_IN_STEAL_ICON;
				SpecialCursor=CURSOR_INTERACTION_ON;

				if(eeMouseDown1()) {
					ARX_INVENTORY_OpenClose(ioSteal);

					if(player.Interface&(INTER_INVENTORY | INTER_INVENTORYALL)) {
						ARX_SOUND_PlayInterface(SND_BACKPACK, 0.9F + 0.2F * rnd());
					}

					if(SecondaryInventory) {
						SendIOScriptEvent(ioSteal, SM_STEAL);

						bForceEscapeFreeLook=true;
					    lOldTruePlayerMouseLook=!TRUE_PLAYER_MOUSELOOK_ON;
					}
				}

				if(DRAGINTER == NULL)
					return;
			}
		}
	}
	
	void draw() {
		m_isSelected = eMouseState == MOUSE_IN_STEAL_ICON;
		HudIconBase::draw();
	}
};

static StealIconGui stealIconGui;


class LevelUpIconGui : public HudIconBase {
private:
	Vec2f m_pos;
	Vec2f m_size;
	bool m_visible;
	
public:
	LevelUpIconGui()
		: HudIconBase()
		, m_pos(0.f, 0.f)
		, m_size(32.f, 32.f)
		, m_visible(false)
	{}
	
	void init() {
		m_tex = TextureContainer::LoadUI("graph/interface/icons/lvl_up");
		arx_assert(m_tex);
		m_size = Vec2f(32.f, 32.f);
	}
	
	void update(const Rectf & parent) {
		m_rect = createChild(parent, Anchor_TopRight, m_size * m_scale, Anchor_BottomRight);
		
		m_visible = (player.Skill_Redistribute) || (player.Attribute_Redistribute);
	}
	
	void updateInput() {
		if(!m_visible)
			return;
		
		m_isSelected = m_rect.contains(Vec2f(DANAEMouse));
		
		if(m_isSelected) {
			SpecialCursor = CURSOR_INTERACTION_ON;
			
			if(eeMouseDown1()) {
				ARX_INTERFACE_BookOpen();
			}
		}
	}
	

	void draw() {
		if(!m_visible)
			return;
		
		HudIconBase::draw();
	}
};

LevelUpIconGui levelUpIconGui;


class PurseIconGui : public HudIconBase {
private:
	Vec2f m_pos;
	Vec2f m_size;
	
	long ulGoldHaloTime;
	
public:
	PurseIconGui()
		: HudIconBase()
		, m_pos()
		, m_size()
		, ulGoldHaloTime(0)
	{}
	
	void init() {
		m_tex = TextureContainer::LoadUI("graph/interface/inventory/gold");
		arx_assert(m_tex);
		m_size = Vec2f(32.f, 32.f);
		
		m_haloColor = Color3f(0.9f, 0.9f, 0.1f);
		
		m_haloActive = false;
		ulGoldHaloTime = 0;
	}
	
	void requestHalo() {
		m_haloActive = true;
		ulGoldHaloTime = 0;
	}
	
	void update(const Rectf & parent) {
		m_rect = createChild(parent, Anchor_TopRight, m_size * m_scale, Anchor_BottomRight);
		
		//A halo is drawn on the character's stats icon (book) when leveling up, for example.
		if(m_haloActive) {
			float fCalc = ulGoldHaloTime + Original_framedelay;
			ulGoldHaloTime = checked_range_cast<unsigned long>(fCalc);
			if(ulGoldHaloTime >= 1000) { // ms
				m_haloActive = false;
			}
		}
	}
	
	void updateInput() {
		m_isSelected = false;
		// gold
		if(player.gold > 0) {
			m_isSelected = m_rect.contains(Vec2f(DANAEMouse));
			
			if(m_isSelected) {
				SpecialCursor = CURSOR_INTERACTION_ON;

				if(   player.gold > 0
				   && !GInput->actionPressed(CONTROLS_CUST_MAGICMODE)
				   && !COMBINE
				   && !COMBINEGOLD
				   && eeMouseDoubleClick1()
				) {
					COMBINEGOLD = true;
				}

				if(!DRAGINTER)
					return;
			}
		}
	}
	
	void draw() {
		HudIconBase::draw();
		
		if(m_isSelected) {
			Vec2f numberPos = m_rect.topLeft();
			numberPos += Vec2f(-30 * m_scale, -15 * m_scale);
			
			ARX_INTERFACE_DrawNumber(numberPos, player.gold, 6, Color::white, m_scale);
		}
	}
};

static PurseIconGui purseIconGui;

void purseIconGuiRequestHalo() {
	purseIconGui.requestHalo();
}


class CurrentTorchIconGui : public HudItem {
private:
	bool m_isActive;
	Rectf m_rect;
	TextureContainer * m_tex;
	Vec2f m_size;
	
public:
	void init() {
		m_size = Vec2f(32.f, 64.f);
	}
	
	bool isVisible() {
		return !(player.Interface & INTER_COMBATMODE) && player.torch;
	}
	
	void updateRect(const Rectf & parent) {
		
		float secondaryInventoryX = InventoryX + 110.f;
		
		m_rect = createChild(parent, Anchor_TopLeft, m_size * m_scale, Anchor_BottomLeft);
		
		if(m_rect.left < secondaryInventoryX) {
			m_rect.move(secondaryInventoryX, 0.f);
		}
	}
	
	void updateInput() {
		if(player.torch) {
			
			if(m_rect.contains(Vec2f(DANAEMouse))) {
				eMouseState=MOUSE_IN_TORCH_ICON;
				SpecialCursor=CURSOR_INTERACTION_ON;
				
				if(!DRAGINTER && !PLAYER_MOUSELOOK_ON && DRAGGING) {
					Entity * io=player.torch;
					player.torch->show=SHOW_FLAG_IN_SCENE;
					ARX_SOUND_PlaySFX(SND_TORCH_END);
					ARX_SOUND_Stop(SND_TORCH_LOOP);
					player.torch=NULL;
					lightHandleGet(torchLightHandle)->exist = 0;
					io->ignition=1;
					Set_DragInter(io);
				} else {
					if(eeMouseDoubleClick1() && !COMBINE) {
						COMBINE = player.torch;
					}

					if(eeMouseUp2()) {
						ARX_PLAYER_ClickedOnTorch(player.torch);
						TRUE_PLAYER_MOUSELOOK_ON = false;
					}
				}
			}
		}
	}
	
	void update() {
		
		if(!isVisible())
			return;
		
		if((player.Interface & INTER_NOTE) && TSecondaryInventory != NULL
		   && (openNote.type() == gui::Note::BigNote || openNote.type() == gui::Note::Book)) {
			m_isActive = false;
			return;
		}
		m_isActive = true;
		
		m_tex = player.torch->inv;
		arx_assert(m_tex);
		
		if(rnd() <= 0.2f) {
			return;
		}
		
		createFireParticle();
	}
	
	void createFireParticle() {
		
		PARTICLE_DEF * pd = createParticle();
		if(!pd) {
			return;
		}
		
		Vec2f pos = m_rect.topLeft() + Vec2f(12.f - rnd() * 3.f, rnd() * 6.f) * m_scale;
		
		pd->special = FIRE_TO_SMOKE;
		pd->ov = Vec3f(pos, 0.0000001f);
		pd->move = Vec3f((1.5f - rnd() * 3.f), -(5.f + rnd() * 1.f), 0.f) * m_scale;
		pd->scale = Vec3f(1.8f, 1.8f, 1.f);
		pd->tolive = Random::get(500, 900);
		pd->tc = fire2;
		pd->rgb = Color3f(1.f, .6f, .5f);
		pd->siz = 14.f * m_scale;
		pd->is2D = true;
	}
	
	void draw() {
		
		if(!isVisible())
			return;
		
		EERIEDrawBitmap(m_rect, 0.001f, m_tex, Color::white);
	}
};

CurrentTorchIconGui currentTorchIconGui;


class ChangeLevelIconGui : public HudItem {
private:
	TextureContainer * m_tex;
	Vec2f m_size;
	
	float m_intensity;
	
public:
	ChangeLevelIconGui()
		: m_tex(NULL)
		, m_intensity(1.f)
	{}
	
	void init() {
		m_tex = TextureContainer::LoadUI("graph/interface/icons/change_lvl");
		arx_assert(m_tex);
		m_size = Vec2f(32.f, 32.f);
	}
	
	bool isVisible() {
		return CHANGE_LEVEL_ICON > -1;
	}
	
	void update(const Rectf & parent) {
		m_rect = createChild(parent, Anchor_TopRight, m_size * m_scale, Anchor_TopRight);
		
		m_intensity = 0.9f - std::sin(arxtime.get_frame_time()*( 1.0f / 50 ))*0.5f+rnd()*( 1.0f / 10 );
		m_intensity = glm::clamp(m_intensity, 0.f, 1.f);
	}
	
	void draw() {
		
		if(!isVisible())
			return;
		
		EERIEDrawBitmap(m_rect, 0.0001f, m_tex, Color3f::gray(m_intensity).to<u8>());
		
	    if(m_rect.contains(Vec2f(DANAEMouse))) {
			SpecialCursor=CURSOR_INTERACTION_ON;
			if(eeMouseUp1()) {
				CHANGE_LEVEL_ICON = 200;
			}
		}
	}
};
ChangeLevelIconGui changeLevelIconGui;

class QuickSaveIconGui {
private:
	//! Time in ms to show the icon
	u32 QUICK_SAVE_ICON_TIME;
	//! Remaining time for the quick sive icon
	unsigned g_quickSaveIconTime;
	
public:
	QuickSaveIconGui()
		: QUICK_SAVE_ICON_TIME(1000)
		, g_quickSaveIconTime(0)
	{}
	
	void show() {
		g_quickSaveIconTime = QUICK_SAVE_ICON_TIME;
	}
	
	void hide() {
		g_quickSaveIconTime = 0;
	}
	
	void update() {
		if(g_quickSaveIconTime) {
			if(g_quickSaveIconTime > unsigned(framedelay)) {
				g_quickSaveIconTime -= unsigned(framedelay);
			} else {
				g_quickSaveIconTime = 0;
			}
		}
	}
	
	void draw() {
		if(!g_quickSaveIconTime) {
			return;
		}
		
		// Flash the icon twice, starting at about 0.7 opacity
		float step = 1.f - float(g_quickSaveIconTime) * (1.f / QUICK_SAVE_ICON_TIME);
		float alpha = std::min(1.f, 0.6f * (std::sin(step * (7.f / 2.f * PI)) + 1.f));
		
		TextureContainer * tex = TextureContainer::LoadUI("graph/interface/icons/menu_main_save");
		arx_assert(tex);
		
		GRenderer->SetRenderState(Renderer::AlphaBlending, true);
		GRenderer->SetBlendFunc(Renderer::BlendSrcColor, Renderer::BlendOne);
		
		Vec2f size = Vec2f(tex->size());
		EERIEDrawBitmap2(Rectf(Vec2f(0, 0), size.x, size.y), 0.f, tex, Color::gray(alpha));
		
		GRenderer->SetRenderState(Renderer::AlphaBlending, false);
	}
};

QuickSaveIconGui quickSaveIconGui = QuickSaveIconGui();


void showQuickSaveIcon() {
	quickSaveIconGui.show();
}

void hideQuickSaveIcon() {
	quickSaveIconGui.hide();
}


class MemorizedRunesHud : public HudIconBase {
private:
	Vec2f m_size;
	int m_count;
	
public:
	MemorizedRunesHud()
		: HudIconBase()
		, m_size()
		, m_count(0)
	{}
	
	void update(const Rectf & parent) {
		int count = 0;
		int count2 = 0;
		for(long j = 0; j < 6; j++) {
			if(player.SpellToMemorize.iSpellSymbols[j] != RUNE_NONE) {
				count++;
			}
			if(SpellSymbol[j] != RUNE_NONE) {
				count2++;
			}
		}
		m_count = std::max(count, count2);
		
		m_size = Vec2f(m_count * 32, 32);
		
		m_rect = createChild(parent, Anchor_TopLeft, m_size * m_scale, Anchor_TopRight);
	}
	
	void draw() {
		Vec2f pos = m_rect.topLeft();
		
		for(int i = 0; i < 6; i++) {
			bool bHalo = false;
			if(SpellSymbol[i] != RUNE_NONE) {
				if(SpellSymbol[i] == player.SpellToMemorize.iSpellSymbols[i]) {
					bHalo = true;
				} else {
					player.SpellToMemorize.iSpellSymbols[i] = SpellSymbol[i];
	
					for(int j = i+1; j < 6; j++) {
						player.SpellToMemorize.iSpellSymbols[j] = RUNE_NONE;
					}
				}
			}
			if(player.SpellToMemorize.iSpellSymbols[i] != RUNE_NONE) {
				
				Vec2f size = Vec2f(32.f, 32.f) * m_scale;
				Rectf rect = Rectf(pos, size.x, size.y);
				
				TextureContainer *tc = gui::necklace.pTexTab[player.SpellToMemorize.iSpellSymbols[i]];
				
				EERIEDrawBitmap2(rect, 0, tc, Color::white);
				
				if(bHalo) {				
					ARX_INTERFACE_HALO_Render(Color3f(0.2f, 0.4f, 0.8f), HALO_ACTIVE, tc->getHalo(), pos, Vec2f(m_scale));
				}
				
				if(!player.hasRune(player.SpellToMemorize.iSpellSymbols[i])) {
					GRenderer->SetBlendFunc(Renderer::BlendInvDstColor, Renderer::BlendOne);
					GRenderer->SetRenderState(Renderer::AlphaBlending, true);
					EERIEDrawBitmap2(rect, 0, cursorMovable, Color3f::gray(.8f).to<u8>());
					GRenderer->SetRenderState(Renderer::AlphaBlending, false);
				}
				pos.x += 32 * m_scale;
			}
		}
		if(float(arxtime) - player.SpellToMemorize.lTimeCreation > 30000) {
			player.SpellToMemorize.bSpell = false;
		}
	}
};

MemorizedRunesHud memorizedRunesHud;


class HealthGauge : public HudItem {
private:
	Vec2f m_size;
	
	Color m_color;
	TextureContainer * m_emptyTex;
	TextureContainer * m_filledTex;
	float m_amount;
public:
	HealthGauge()
		: m_size(33.f, 80.f)
		, m_emptyTex(NULL)
		, m_filledTex(NULL)
		, m_amount(0.f)
	{}
	
	void init() {
		m_emptyTex = TextureContainer::LoadUI("graph/interface/bars/empty_gauge_red");
		m_filledTex = TextureContainer::LoadUI("graph/interface/bars/filled_gauge_red");
		arx_assert(m_emptyTex);
		arx_assert(m_filledTex);
	}
	
	void updateRect(const Rectf & parent) {
		m_rect = createChild(parent, Anchor_BottomLeft, m_size * m_scale, Anchor_BottomLeft);
	}
	
	void update() {
		
		m_amount = (float)player.lifePool.current/(float)player.Full_maxlife;
		
		if(player.poison > 0.f) {
			float val = std::min(player.poison, 0.2f) * 255.f * 5.f;
			long g = val;
			m_color = Color(u8(255 - g), u8(g) , 0);
		} else {
			m_color = Color::red;
		}
	}
	
	void updateInput(const Vec2f & mousePos) {
		if(!(player.Interface & INTER_COMBATMODE)) {
			if(m_rect.contains(mousePos)) {
				if(eeMouseDown1()) {
					std::stringstream ss;
					ss << checked_range_cast<int>(player.lifePool.current);
					ARX_SPEECH_Add(ss.str());
				}
			}
		}
	}
	
	void draw() {
		
		EERIEDrawBitmap2DecalY(m_rect, 0.f, m_filledTex, m_color, (1.f - m_amount));
		EERIEDrawBitmap(m_rect, 0.001f, m_emptyTex, Color::white);
	}
};
HealthGauge healthGauge;

class ManaGauge : public HudItem {
private:
	Vec2f m_size;
	
	TextureContainer * m_emptyTex;
	TextureContainer * m_filledTex;
	float m_amount;
	
public:
	ManaGauge()
		: HudItem()
		, m_size(33.f, 80.f)
		, m_emptyTex(NULL)
		, m_filledTex(NULL)
		, m_amount(0.f)
	{}
	
	void init() {
		m_emptyTex = TextureContainer::LoadUI("graph/interface/bars/empty_gauge_blue");
		m_filledTex = TextureContainer::LoadUI("graph/interface/bars/filled_gauge_blue");
		arx_assert(m_emptyTex);
		arx_assert(m_filledTex);
	}
	
	void update(const Rectf & parent) {
		
		m_rect = createChild(parent, Anchor_BottomRight, m_size * m_scale, Anchor_BottomRight);
		
		m_amount = player.manaPool.current / player.Full_maxmana;
	}
	
	void updateInput(const Vec2f & mousePos) {
		if(!(player.Interface & INTER_COMBATMODE)) {
			if(m_rect.contains(mousePos)) {
				if(eeMouseDown1()) {
					std::stringstream ss;
					ss << checked_range_cast<int>(player.manaPool.current);
					ARX_SPEECH_Add(ss.str());
				}
			}
		}
	}
	
	void draw() {
		
		EERIEDrawBitmap2DecalY(m_rect, 0.f, m_filledTex, Color::white, (1.f - m_amount));
		EERIEDrawBitmap(m_rect, 0.001f, m_emptyTex, Color::white);
	}
};
ManaGauge manaGauge;

//The cogwheel icon that shows up when switching from mouseview to interaction mode.
class MecanismIcon : public HudItem {
private:
	Vec2f m_iconSize;
	TextureContainer * m_tex;
	Color m_color;
	long m_timeToDraw;
	long m_nbToDraw;
	
public:
	MecanismIcon()
		: HudItem()
		, m_iconSize(32.f, 32.f)
		, m_tex(NULL)
		, m_timeToDraw(0)
		, m_nbToDraw(0)
	{}
	
	void init() {
		m_tex = TextureContainer::LoadUI("graph/interface/cursors/mecanism");
		arx_assert(m_tex);
		
		reset();
	}
	
	void reset() {
		m_timeToDraw = 0;
		m_nbToDraw = 0;
	}
	
	void update() {
		m_color = Color::white;
		if(m_timeToDraw > 300) {
			m_color = Color::black;
			if(m_timeToDraw > 400) {
				m_timeToDraw=0;
				m_nbToDraw++;
			}
		}
		m_timeToDraw += static_cast<long>(framedelay);
		
		m_rect = createChild(Rectf(g_size), Anchor_TopLeft, m_iconSize * m_scale, Anchor_TopLeft);
	}
	
	void draw() {
		if(m_nbToDraw >= 3) {
			return;
		}
		
		EERIEDrawBitmap(m_rect, 0.01f, m_tex, m_color);
	}
};
MecanismIcon mecanismIcon;

void mecanismIconReset() {
	mecanismIcon.reset();
}

class ScreenArrows : public HudItem {
private:
	Vec2f m_horizontalArrowSize;
	Vec2f m_verticalArrowSize;
	
	Rectf m_left;
	Rectf m_right;
	Rectf m_top;
	Rectf m_bottom;
	
	TextureContainer * m_arrowLeftTex;
	
	float fArrowMove;
public:
	ScreenArrows()
		: HudItem()
		, m_horizontalArrowSize(8, 16)
		, m_verticalArrowSize(16, 8)
		, m_arrowLeftTex(NULL)
		, fArrowMove(0.f)
	{}
	
	void init() {
		m_arrowLeftTex = TextureContainer::LoadUI("graph/interface/icons/arrow_left");
		arx_assert(m_arrowLeftTex);
	}
	
	void update() {
		fArrowMove += .5f * framedelay;
		if(fArrowMove > 180.f) {
			fArrowMove=0.f;
		}
		
		float fMove = glm::abs(glm::sin(glm::radians(fArrowMove))) * m_horizontalArrowSize.x * m_scale * .5f;
		
		const Rectf parent = createChild(Rectf(g_size), Anchor_Center, Vec2f(g_size.size()) - Vec2f(fMove), Anchor_Center);
		m_left   = createChild(parent, Anchor_LeftCenter,   m_horizontalArrowSize * m_scale, Anchor_LeftCenter);
		m_right  = createChild(parent, Anchor_RightCenter,  m_horizontalArrowSize * m_scale, Anchor_RightCenter);
		m_top    = createChild(parent, Anchor_TopCenter,    m_verticalArrowSize * m_scale,   Anchor_TopCenter);
		m_bottom = createChild(parent, Anchor_BottomCenter, m_verticalArrowSize * m_scale,   Anchor_BottomCenter);
	}
	
	void draw() {
		Color lcolor = Color3f::gray(.5f).to<u8>();

		EERIEDrawBitmap(m_left, 0.01f, m_arrowLeftTex, lcolor);
		EERIEDrawBitmapUVs(m_right,  .01f, m_arrowLeftTex, lcolor, Vec2f(1.f, 0.f), Vec2f(0.f, 0.f), Vec2f(1.f, 1.f), Vec2f(0.f, 1.f));
		EERIEDrawBitmapUVs(m_top,    .01f, m_arrowLeftTex, lcolor, Vec2f(0.f, 1.f), Vec2f(0.f, 0.f), Vec2f(1.f, 1.f), Vec2f(1.f, 0.f));
		EERIEDrawBitmapUVs(m_bottom, .01f, m_arrowLeftTex, lcolor, Vec2f(1.f, 1.f), Vec2f(1.f, 0.f), Vec2f(0.f, 1.f), Vec2f(0.f, 0.f));
	}
};
ScreenArrows screenArrows;

class PrecastSpellsGui : public HudItem {
private:
	struct PrecastSpellIconSlot {
		Rectf m_rect;
		TextureContainer * m_tc;
		Color m_color;
		PrecastHandle m_precastIndex;
		
		void update(const Rectf & rect, TextureContainer * tc, Color color, PrecastHandle precastIndex) {
			m_rect = rect;
			m_tc = tc;
			m_color = color;
			m_precastIndex = precastIndex;
		}
		
		void updateInput() {
			if(m_rect.contains(Vec2f(DANAEMouse))) {
				SpecialCursor = CURSOR_INTERACTION_ON;
				
				if(eeMouseUp1()) {
					if(Precast[m_precastIndex].typ >= 0) {
						ARX_SPEECH_Add(spellicons[Precast[m_precastIndex].typ].name);
					}
				}
				
				if(eeMouseDoubleClick1()) {
					ARX_SPELLS_Precast_Launch(m_precastIndex);
				}
			}
		}
		
		void draw() {
			EERIEDrawBitmap(m_rect, 0.01f, m_tc, m_color);
			
			GRenderer->SetBlendFunc(Renderer::BlendZero, Renderer::BlendOne);
			
			Rectf rect2 = m_rect;
			rect2.move(-1, -1);
			EERIEDrawBitmap(rect2, 0.0001f, m_tc, m_color);
			
			Rectf rect3 = m_rect;
			rect3.move(1, 1);
			EERIEDrawBitmap(rect3, 0.0001f, m_tc, m_color);
			
			GRenderer->SetBlendFunc(Renderer::BlendOne, Renderer::BlendOne);
		}
	};
	std::vector<PrecastSpellIconSlot> m_icons;
	
	
	Vec2f m_iconSize;
	
public:
	
	PrecastSpellsGui()
		: HudItem()
	{
		m_iconSize = Vec2f(48, 48) / Vec2f(2);
	}
	
	bool isVisible() {
		return !(player.Interface & INTER_INVENTORYALL) && !(player.Interface & INTER_MAP);
	}
	
	void updateRect(const Rectf & parent) {
		
		Vec2f size = m_iconSize * Vec2f(Precast.size(), 1);
		
		m_rect = createChild(parent, Anchor_BottomRight, size * m_scale, Anchor_BottomLeft);
	}
	
	void update() {
		m_icons.clear();
		
		if(!isVisible())
			return;
		
		float intensity = 1.f - PULSATE * 0.5f;
		intensity = glm::clamp(intensity, 0.f, 1.f);
		
		
		for(size_t i = 0; i < Precast.size(); i++) {
			
			PRECAST_STRUCT & precastSlot = Precast[i];
			
			float val = intensity;
			
			if(precastSlot.launch_time > 0 && (float(arxtime) >= precastSlot.launch_time)) {
				float tt = (float(arxtime) - precastSlot.launch_time) * (1.0f/1000);
				
				if(tt > 1.f)
					tt = 1.f;
				
				val *= (1.f - tt);
			}
	
			Color color = Color3f(0, val * (1.0f/2), val).to<u8>();
			
			Rectf childRect = createChild(m_rect, Anchor_BottomLeft, m_iconSize * m_scale, Anchor_BottomLeft);
			childRect.move(i * 33 * m_scale, 0);
			
			SpellType typ = precastSlot.typ;
			
			TextureContainer * tc = spellicons[typ].tc;
			arx_assert(tc);
			
			PrecastSpellIconSlot icon;
			icon.update(childRect, tc, color, PrecastHandle(i));
			
			if(!(player.Interface & INTER_COMBATMODE))
				icon.updateInput();
			
			m_icons.push_back(icon);
		}
	}
	
	void draw() {
		GRenderer->SetBlendFunc(Renderer::BlendOne, Renderer::BlendOne);
		GRenderer->SetRenderState(Renderer::AlphaBlending, true);
		
		std::vector<PrecastSpellIconSlot>::iterator itr;
		for(itr = m_icons.begin(); itr != m_icons.end(); ++itr) {
			itr->draw();
		}
	}
};
PrecastSpellsGui precastSpellsGui;


class ActiveSpellsGui : public HudItem {
private:

	struct ActiveSpellIconSlot {
		Rectf m_rect;
		TextureContainer * m_tc;
		Color m_color;
		SpellHandle spellIndex;
		bool m_flicker;
		bool m_abortable;
		
		void updateInput(const Vec2f & mousePos) {
			
			if(!m_abortable)
				return;
			
			if(m_rect.contains(mousePos)) {
				SpecialCursor = CURSOR_INTERACTION_ON;
				
				if(eeMouseUp1()) {
					if(spells[spellIndex]->m_type >= 0) {
						ARX_SPEECH_Add(spellicons[spells[spellIndex]->m_type].name);
					}
				}
				
				if(eeMouseDoubleClick1()) {
					ARX_SOUND_PlaySFX(SND_MAGIC_FIZZLE);
					spells.endSpell(spells[spellIndex]);
				}
			}
		}
		
		void draw() {
			
			if(!m_flicker)
				return;
			
			EERIEDrawBitmap(m_rect, 0.01f, m_tc, m_color);
		}
	};
	
public:
	ActiveSpellsGui()
		: HudItem()
		, m_texUnknown(NULL)
	{}
	
	void init() {
		m_texUnknown = TextureContainer::Load("graph/interface/icons/spell_unknown");
		arx_assert(m_texUnknown);
		
		m_slotSize = Vec2f(24.f, 24.f);
		m_spacerSize = Vec2f(60.f, 50.f);
		m_slotSpacerSize = Vec2f(0.f, 9.f);
	}
	
	void update(Rectf parent) {
		
		float intensity = 1.f - PULSATE * 0.5f;
		intensity = glm::clamp(intensity, 0.f, 1.f);
		
		m_slots.clear();
		
		spellsByPlayerUpdate(intensity);
		spellsOnPlayerUpdate(intensity);
		
		Rectf spacer = createChild(parent, Anchor_TopRight, m_spacerSize * m_scale, Anchor_TopRight);
		Rectf siblingRect = spacer;
		
		BOOST_FOREACH(ActiveSpellIconSlot & slot, m_slots) {
			
			Rectf slotRect = createChild(siblingRect, Anchor_BottomLeft, m_slotSize * m_scale, Anchor_TopLeft);
			Rectf slotSpacer = createChild(slotRect, Anchor_BottomLeft, m_slotSpacerSize * m_scale, Anchor_TopLeft);
			siblingRect = slotSpacer;
			
			slot.m_rect = slotRect;
		}
	}
	
	void updateInput(const Vec2f & mousePos) {
		
		BOOST_FOREACH(ActiveSpellIconSlot & slot, m_slots) {
			slot.updateInput(mousePos);
		}
	}
	
	void draw() {
		
		GRenderer->SetBlendFunc(Renderer::BlendOne, Renderer::BlendOne);
		GRenderer->SetRenderState(Renderer::AlphaBlending, true);
		
		BOOST_FOREACH(ActiveSpellIconSlot & slot, m_slots) {
			slot.draw();
		}
	}
	
private:
	TextureContainer * m_texUnknown;
	Vec2f m_slotSize;
	Vec2f m_spacerSize;
	Vec2f m_slotSpacerSize;
	
	std::vector<ActiveSpellIconSlot> m_slots;
	
	void spellsByPlayerUpdate(float intensity) {
		for(size_t i = 0; i < MAX_SPELLS; i++) {
			SpellBase * spell = spells[SpellHandle(i)];
			
			if(   spell
			   && spell->m_caster == PlayerEntityHandle
			   && spellicons[spell->m_type].m_hasDuration
			) {
				ManageSpellIcon(*spell, intensity, false);
			}
		}
	}
	
	void spellsOnPlayerUpdate(float intensity) {
		for(size_t i = 0; i < MAX_SPELLS; i++) {
			SpellBase * spell = spells[SpellHandle(i)];
			if(!spell)
				continue;
			
			if(std::find(spell->m_targets.begin(), spell->m_targets.end(), PlayerEntityHandle) == spell->m_targets.end()) {
				continue;
			}
			
			if(spell->m_caster != PlayerEntityHandle && spellicons[spell->m_type].m_hasDuration) {
				ManageSpellIcon(*spell, intensity, true);
			}
		}
	}
	
	void ManageSpellIcon(SpellBase & spell, float intensity, bool flag) {
		
		
		Color color = (flag) ? Color3f(intensity, 0, 0).to<u8>() : Color3f::gray(intensity).to<u8>();
		
		bool flicker = true;
		
		if(spell.m_hasDuration) {
			if(player.manaPool.current < 20 || spell.m_timcreation + spell.m_duration - float(arxtime) < 2000) {
				if(ucFlick&1)
					flicker = false;
			}
		} else {
			if(player.manaPool.current<20) {
				if(ucFlick&1)
					flicker = false;
			}
		}
		
		if(spell.m_type >= 0 && (size_t)spell.m_type < SPELL_TYPES_COUNT) {
		
			ActiveSpellIconSlot slot;
			slot.m_tc = spellicons[spell.m_type].tc;
			slot.m_color = color;
			slot.spellIndex = spell.m_thisHandle;
			slot.m_flicker = flicker;
			slot.m_abortable = (!flag && !(player.Interface & INTER_COMBATMODE));
			
			m_slots.push_back(slot);
		}
	}
};
ActiveSpellsGui activeSpellsGui = ActiveSpellsGui();

/*!
 * \brief Damaged Equipment Drawing
 */
class DamagedEquipmentGui : public HudItem {
private:
	Vec2f m_size;
	
	TextureContainer * iconequip[5];
	
	Color m_colors[5];
	
public:
	DamagedEquipmentGui()
		: HudItem()
		, m_size(64.f, 64.f)
	{
		iconequip[0] = NULL;
		iconequip[1] = NULL;
		iconequip[2] = NULL;
		iconequip[3] = NULL;
		iconequip[4] = NULL;
	}
	
	void init() {
		iconequip[0] = TextureContainer::LoadUI("graph/interface/icons/equipment_sword");
		iconequip[1] = TextureContainer::LoadUI("graph/interface/icons/equipment_shield");
		iconequip[2] = TextureContainer::LoadUI("graph/interface/icons/equipment_helm");
		iconequip[3] = TextureContainer::LoadUI("graph/interface/icons/equipment_chest");
		iconequip[4] = TextureContainer::LoadUI("graph/interface/icons/equipment_leggings");
		arx_assert(iconequip[0]);
		arx_assert(iconequip[1]);
		arx_assert(iconequip[2]);
		arx_assert(iconequip[3]);
		arx_assert(iconequip[4]);
	}
	
	void updateRect(const Rectf & parent) {
		m_rect = createChild(parent, Anchor_BottomRight, m_size * m_scale, Anchor_BottomLeft);
	}
	
	void update() {
		if(cinematicBorder.isActive() || BLOCK_PLAYER_CONTROLS)
			return;
	
		if(player.Interface & INTER_INVENTORYALL)
			return;
		
		for(long i = 0; i < 5; i++) {
			m_colors[i] = Color::black;
			
			long eq=-1;

			switch (i) {
				case 0: eq = EQUIP_SLOT_WEAPON; break;
				case 1: eq = EQUIP_SLOT_SHIELD; break;
				case 2: eq = EQUIP_SLOT_HELMET; break;
				case 3: eq = EQUIP_SLOT_ARMOR; break;
				case 4: eq = EQUIP_SLOT_LEGGINGS; break;
			}
			
			if(ValidIONum(player.equiped[eq])) {
				Entity *io = entities[player.equiped[eq]];
				float ratio = io->durability / io->max_durability;
				
				if(ratio <= 0.5f)
					m_colors[i] = Color3f(1.f-ratio, ratio, 0).to<u8>();
			}
		}
	}
	
	void draw() {
		
		GRenderer->SetRenderState(Renderer::AlphaBlending, true);
		GRenderer->SetBlendFunc(Renderer::BlendOne, Renderer::BlendOne);
		
		GRenderer->SetCulling(Renderer::CullNone);
		GRenderer->SetRenderState(Renderer::DepthWrite, true);
		GRenderer->SetRenderState(Renderer::Fog, false);
		
		for(long i = 0; i < 5; i++) {
			if(m_colors[i] == Color::black)
				continue;
			
			EERIEDrawBitmap2(m_rect, 0.001f, iconequip[i], m_colors[i]);
		}
		
		GRenderer->SetRenderState(Renderer::AlphaBlending, false);
	}
};
DamagedEquipmentGui damagedEquipmentGui;

extern float CURRENT_PLAYER_COLOR;

/*!
 * \brief Stealth Gauge Drawing
 */
class StealthGauge : public HudItem {
private:
	TextureContainer * stealth_gauge_tc;
	
	bool m_visible;
	Color m_color;
	Vec2f m_size;
	
public:
	StealthGauge()
		: HudItem()
		, stealth_gauge_tc(NULL)
		, m_visible(false)
	{}
	
	void init() {
		stealth_gauge_tc = TextureContainer::LoadUI("graph/interface/icons/stealth_gauge");
		arx_assert(stealth_gauge_tc);
		m_size = Vec2f(32.f, 32.f);
	}
	
	void update(const Rectf & parent) {
		m_rect = createChild(parent, Anchor_TopRight, m_size * m_scale, Anchor_BottomLeft);
		
		m_visible = false;
		
		if(!cinematicBorder.isActive()) {
			float v=GetPlayerStealth();
	
			if(CURRENT_PLAYER_COLOR < v) {
				float t = v - CURRENT_PLAYER_COLOR;
	
				if(t >= 15)
					v = 1.f;
				else
					v = (t*( 1.0f / 15 ))* 0.9f + 0.1f;
				
				m_color = Color3f::gray(v).to<u8>();
				
				m_visible = true;
			}
		}
	}
	
	void draw() {
		if(!m_visible)
			return;
		
		GRenderer->SetRenderState(Renderer::AlphaBlending, true);
		GRenderer->SetBlendFunc(Renderer::BlendOne, Renderer::BlendOne);
		EERIEDrawBitmap(m_rect, 0.01f, stealth_gauge_tc, m_color);
		GRenderer->SetRenderState(Renderer::AlphaBlending, false);
	}
};
StealthGauge stealthGauge;


void hudElementsInit() {
	changeLevelIconGui.init();
	currentTorchIconGui.init();
	activeSpellsGui.init();
	damagedEquipmentGui.init();
	mecanismIcon.init();
	
	stealthGauge.init();
	screenArrows.init();
	
	healthGauge.init();
	manaGauge.init();
	bookIconGui.init();
	backpackIconGui.init();
	levelUpIconGui.init();
	stealIconGui.init();
	g_secondaryInventoryHud.init();
	g_playerInventoryHud.init();
	
	purseIconGui.init();
	
	hitStrengthGauge.init();
	
	//setHudScale(2);
}

void setHudScale(float scale) {
	hitStrengthGauge.setScale(scale);
	healthGauge.setScale(scale);
	stealIconGui.setScale(scale);
	currentTorchIconGui.setScale(scale);
	
	manaGauge.setScale(scale);
	backpackIconGui.setScale(scale);
	bookIconGui.setScale(scale);
	purseIconGui.setScale(scale);
	levelUpIconGui.setScale(scale);
	
	changeLevelIconGui.setScale(scale);
	memorizedRunesHud.setScale(scale);
	activeSpellsGui.setScale(scale);
	
	mecanismIcon.setScale(scale);
	screenArrows.setScale(scale);
	
	stealthGauge.setScale(scale);
	damagedEquipmentGui.setScale(scale);
	precastSpellsGui.setScale(scale);
	
	g_playerInventoryHud.setScale(scale);
	g_secondaryInventoryHud.setScale(scale);
}

void ArxGame::drawAllInterface() {
	
	const Vec2f mousePos = Vec2f(DANAEMouse);
	
	Rectf hudSlider = Rectf(g_size);
	hudSlider.left  -= lSLID_VALUE;
	hudSlider.right += lSLID_VALUE;
	
	
	hitStrengthGauge.updateRect(Rectf(g_size));
	hitStrengthGauge.update();
	
	g_secondaryInventoryHud.update();
	g_playerInventoryHud.update();
	mecanismIcon.update();
	screenArrows.update();
	
	changeLevelIconGui.update(Rectf(g_size));
	memorizedRunesHud.update(changeLevelIconGui.rect());
	
	quickSaveIconGui.update();
	
	
	Vec2f anchorPos = g_playerInventoryHud.anchorPosition();
	
	Rectf spacer;
	spacer.left = std::max(InventoryX + 160, healthGauge.rect().right);
	spacer.bottom = anchorPos.y;
	spacer.top = spacer.bottom - 30;
	spacer.right = spacer.left + 20;
	
	stealthGauge.update(spacer);
	
	damagedEquipmentGui.updateRect(stealthGauge.rect());
	damagedEquipmentGui.update();
	
	precastSpellsGui.updateRect(damagedEquipmentGui.rect());
	precastSpellsGui.update();
	
	
	GRenderer->GetTextureStage(0)->setMinFilter(TextureStage::FilterLinear);
	GRenderer->GetTextureStage(0)->setMagFilter(TextureStage::FilterNearest);
	GRenderer->GetTextureStage(0)->setWrapMode(TextureStage::WrapClamp);

	if(player.Interface & INTER_COMBATMODE) {
		hitStrengthGauge.draw();
	}	
	
	g_secondaryInventoryHud.draw();
	g_playerInventoryHud.draw();
	
	if(FlyingOverIO 
		&& !(player.Interface & INTER_COMBATMODE)
		&& !GInput->actionPressed(CONTROLS_CUST_MAGICMODE)
		&& (!PLAYER_MOUSELOOK_ON || !config.input.autoReadyWeapon)
	  ) {
		if((FlyingOverIO->ioflags & IO_ITEM) && !DRAGINTER && SecondaryInventory) {
			DrawItemPrice();
		}
		SpecialCursor=CURSOR_INTERACTION_ON;
	}
	
	healthGauge.updateRect(hudSlider);
	healthGauge.updateInput(mousePos);
	healthGauge.update();
	
	stealIconGui.updateRect(healthGauge.rect());
	
	damagedEquipmentGui.draw();
	
	if(!(player.Interface & INTER_COMBATMODE) && (player.Interface & INTER_MINIBACK)) {
		
		if(player.Interface & INTER_STEAL) {
			stealIconGui.draw();			
		}
	}
	
	currentTorchIconGui.updateRect(stealIconGui.rect());
	currentTorchIconGui.update();
	currentTorchIconGui.draw();
	
	changeLevelIconGui.draw();
	
	quickSaveIconGui.draw();
	stealthGauge.draw();

	if((player.Interface & INTER_MAP) && !(player.Interface & INTER_COMBATMODE)) {
		ARX_INTERFACE_ManageOpenedBook();
		
		GRenderer->SetRenderState(Renderer::DepthWrite, true);
		
		if((player.Interface & INTER_MAP) && !(player.Interface & INTER_COMBATMODE)) {
			if(g_guiBookCurrentTopTab == BOOKMODE_SPELLS) {
				gui::ARX_INTERFACE_ManageOpenedBook_Finish(mousePos);
				ARX_INTERFACE_ManageOpenedBook_SpellsDraw();
			}
		}
	}
	
	if(CurrSpellSymbol || player.SpellToMemorize.bSpell) {
		memorizedRunesHud.draw();
	}
	

	
	
	if(player.Interface & INTER_LIFE_MANA) {
		manaGauge.update(hudSlider);
		manaGauge.updateInput(mousePos);
		manaGauge.draw();
		
		healthGauge.draw();
		
		if(bRenderInCursorMode) {
			GRenderer->SetRenderState(Renderer::AlphaBlending, true);
			GRenderer->SetBlendFunc(Renderer::BlendOne, Renderer::BlendOne);
			if(!MAGICMODE) {
				mecanismIcon.draw();
			}
			screenArrows.draw();
			GRenderer->SetRenderState(Renderer::AlphaBlending, false);
		}
	}
	
	if(!(player.Interface & INTER_COMBATMODE) && (player.Interface & INTER_MINIBACK)) {
		
		{
		Rectf spacer = createChild(manaGauge.rect(), Anchor_TopRight, Vec2f(0, 3), Anchor_BottomRight);
		backpackIconGui.update(spacer);
		}
		
		{
		Rectf spacer = createChild(backpackIconGui.rect(), Anchor_TopRight, Vec2f(0, 3), Anchor_BottomRight);
		bookIconGui.update(spacer);
		}
		
		{
		Rectf spacer = createChild(bookIconGui.rect(), Anchor_TopRight, Vec2f(0, 3), Anchor_BottomRight);
		purseIconGui.update(spacer);
		}
		
		{
		Rectf spacer = createChild(purseIconGui.rect(), Anchor_TopRight, Vec2f(0, 3), Anchor_BottomRight);
		levelUpIconGui.update(spacer);
		}
		
		backpackIconGui.draw();
		bookIconGui.draw();
		
		// Draw/Manage Gold Purse Icon
		if(player.gold > 0) {
			purseIconGui.draw();
		}
		
		levelUpIconGui.draw();
	}
	
	GRenderer->GetTextureStage(0)->setMinFilter(TextureStage::FilterLinear);
	GRenderer->GetTextureStage(0)->setMagFilter(TextureStage::FilterLinear);
	GRenderer->GetTextureStage(0)->setWrapMode(TextureStage::WrapRepeat);
	
	precastSpellsGui.draw();
	activeSpellsGui.update(hudSlider);
	activeSpellsGui.updateInput(mousePos);
	activeSpellsGui.draw();
}


void hudUpdateInput() {
	if(!BLOCK_PLAYER_CONTROLS) {
		if(!(player.Interface & INTER_COMBATMODE)) {
			if(!TRUE_PLAYER_MOUSELOOK_ON) {
				
				currentTorchIconGui.updateInput();
				levelUpIconGui.updateInput();
				purseIconGui.updateInput();
				bookIconGui.updateInput();
				
				backpackIconGui.updateInput();
				
				
			}
			stealIconGui.updateInput();
		}
	}
}
