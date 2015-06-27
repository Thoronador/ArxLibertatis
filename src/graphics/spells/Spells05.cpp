/*
 * Copyright 2011-2012 Arx Libertatis Team (see the AUTHORS file)
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
/* Based on:
===========================================================================
ARX FATALIS GPL Source Code
Copyright (C) 1999-2010 Arkane Studios SA, a ZeniMax Media company.

This file is part of the Arx Fatalis GPL Source Code ('Arx Fatalis Source Code'). 

Arx Fatalis Source Code is free software: you can redistribute it and/or modify it under the terms of the GNU General Public 
License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

Arx Fatalis Source Code is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied 
warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with Arx Fatalis Source Code.  If not, see 
<http://www.gnu.org/licenses/>.

In addition, the Arx Fatalis Source Code is also subject to certain additional terms. You should have received a copy of these 
additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Arx 
Fatalis Source Code. If not, please request a copy in writing from Arkane Studios at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing Arkane Studios, c/o 
ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.
===========================================================================
*/

#include "graphics/spells/Spells05.h"

#include <climits>
#include <cmath>

#include "animation/AnimationRender.h"

#include "core/Config.h"
#include "core/Core.h"
#include "core/GameTime.h"

#include "game/Damage.h"
#include "game/EntityManager.h"
#include "game/Player.h"
#include "game/Spells.h"

#include "graphics/Math.h"
#include "graphics/data/TextureContainer.h"
#include "graphics/effects/SpellEffects.h"
#include "graphics/effects/Fog.h"
#include "graphics/particle/ParticleEffects.h"
#include "graphics/particle/Particle.h"
#include "graphics/particle/ParticleManager.h"
#include "graphics/particle/ParticleParams.h"
#include "graphics/texture/TextureStage.h"

#include "scene/Interactive.h"
#include "scene/Light.h"
#include "scene/Object.h"

#include <list>

extern ParticleManager * pParticleManager;


static void LaunchPoisonExplosion(const Vec3f & aePos) {
	
	// système de partoches pour l'explosion
	ParticleSystem * pPS = new ParticleSystem();
	ParticleParams cp = ParticleParams();
	cp.m_nbMax = 80; 
	cp.m_life = 1500;
	cp.m_lifeRandom = 500;
	cp.m_pos = Vec3f(5);
	cp.m_direction = Vec3f(0.f, 1.f, 0.f);
	cp.m_angle = glm::radians(360.f);
	cp.m_speed = 200;
	cp.m_speedRandom = 0;
	cp.m_gravity = Vec3f(0, 17, 0);
	cp.m_flash = 0;
	cp.m_rotation = 1.0f / (101 - 80);
	cp.m_rotationRandomDirection = true;
	cp.m_rotationRandomStart = true;

	cp.m_startSegment.m_size = 5;
	cp.m_startSegment.m_sizeRandom = 3;
	cp.m_startSegment.m_color = Color(0, 76, 0, 0).to<float>();
	cp.m_startSegment.m_colorRandom = Color(0, 0, 0, 150).to<float>();

	cp.m_endSegment.m_size = 30;
	cp.m_endSegment.m_sizeRandom = 5;
	cp.m_endSegment.m_color = Color(0, 0, 0, 0).to<float>();
	cp.m_endSegment.m_colorRandom = Color(0, 25, 0, 20).to<float>();

	cp.m_blendMode = RenderMaterial::AlphaAdditive;
	cp.m_freq = -1;
	cp.m_texture.set("graph/particles/big_greypouf", 0, 200);
	cp.m_spawnFlags = 0;
	cp.m_looping = false;
	
	pPS->SetParams(cp);
	pPS->SetPos(aePos);
	pPS->Update(0);

	std::list<Particle *>::iterator i;

	for(i = pPS->listParticle.begin(); i != pPS->listParticle.end(); ++i) {
		Particle * pP = *i;

		if(pP->isAlive()) {
			pP->p3Velocity = glm::clamp(pP->p3Velocity, Vec3f(0, -100, 0), Vec3f(0, 100, 0));
		}
	}

	arx_assert(pParticleManager);
	pParticleManager->AddSystem(pPS);
}


CPoisonProjectile::CPoisonProjectile()
	: eSrc(Vec3f_ZERO)
	, lightIntensityFactor(1.f)
	, fBetaRadCos(0.f)
	, fBetaRadSin(0.f)
	, bOk(false)
	, fTrail(-1.f)
{
	SetDuration(2000);
	ulCurrentTime = ulDuration + 1;
}

void CPoisonProjectile::Create(Vec3f _eSrc, float _fBeta)
{
	SetDuration(ulDuration);
	
	float fBetaRad = glm::radians(_fBeta);
	fBetaRadCos = glm::cos(fBetaRad);
	fBetaRadSin = glm::sin(fBetaRad);

	eSrc = _eSrc;

	bOk = false;

	eMove = Vec3f(-fBetaRadSin * 2, 0.f, fBetaRadCos * 2); 

	Vec3f tempHit;
	Vec3f dest = eSrc;

	int i = 0;
	while(Visible(eSrc, dest, &tempHit) && i < 20) {
		dest.x -= fBetaRadSin * 50;
		dest.z += fBetaRadCos * 50;

		i++;
	}

	dest.y += 0.f;

	pathways[0] = eSrc;
	pathways[9] = dest;
	
	Split(pathways, 0, 9, Vec3f(10 * fBetaRadCos, 10, 10 * fBetaRadSin));
	
	fTrail = -1;

	//-------------------------------------------------------------------------
	// système de partoches
	ParticleParams cp = ParticleParams();
	cp.m_nbMax = 5;
	cp.m_life = 2000;
	cp.m_lifeRandom = 1000;
	cp.m_pos = Vec3f_ZERO;
	cp.m_direction = -eMove * 0.1f;
	cp.m_angle = 0;
	cp.m_speed = 10;
	cp.m_speedRandom = 10;
	cp.m_gravity = Vec3f_ZERO;
	cp.m_flash = 21 * (1.f/100);
	cp.m_rotation = 1.0f / (101 - 80);
	cp.m_rotationRandomDirection = true;
	cp.m_rotationRandomStart = true;

	cp.m_startSegment.m_size = 5;
	cp.m_startSegment.m_sizeRandom = 3;
	cp.m_startSegment.m_color = Color(0, 50, 0, 40).to<float>();
	cp.m_startSegment.m_colorRandom = Color(0, 100, 0, 50).to<float>();

	cp.m_endSegment.m_size = 8;
	cp.m_endSegment.m_sizeRandom = 13;
	cp.m_endSegment.m_color = Color(0, 60, 0, 40).to<float>();
	cp.m_endSegment.m_colorRandom = Color(0, 100, 0, 50).to<float>();

	cp.m_blendMode = RenderMaterial::Screen;
	cp.m_freq = -1;
	cp.m_texture.set("graph/particles/big_greypouf", 0, 200);
	cp.m_spawnFlags = 0;
	
	pPS.SetParams(cp);
	pPS.SetPos(eSrc);
	pPS.Update(0);
}

void CPoisonProjectile::Update(float timeDelta)
{
	if(ulCurrentTime <= 2000) {
		ulCurrentTime += timeDelta;
	}

	// on passe de 5 à 100 partoches en 1.5secs
	if(ulCurrentTime < 750) {
		pPS.m_parameters.m_nbMax = 2;
		pPS.Update(timeDelta);
	} else {
		if(!bOk) {
			bOk = true;

			// go
			ParticleParams cp = ParticleParams();
			cp.m_nbMax = 100;
			cp.m_life = 500;
			cp.m_lifeRandom = 300;
			cp.m_pos = Vec3f(fBetaRadSin * 20, 0.f, fBetaRadCos * 20);

			cp.m_direction = -eMove * 0.1f;

			cp.m_angle = glm::radians(4.f);
			cp.m_speed = 150;
			cp.m_speedRandom = 50;//15;
			cp.m_gravity = Vec3f(0, 10, 0);
			cp.m_flash = 0;
			cp.m_rotation = 1.0f / (101 - 80);
			cp.m_rotationRandomDirection = true;
			cp.m_rotationRandomStart = true;

			cp.m_startSegment.m_size = 2;
			cp.m_startSegment.m_sizeRandom = 2;
			cp.m_startSegment.m_color = Color(0, 39, 0, 100).to<float>();
			cp.m_startSegment.m_colorRandom = Color(50, 21, 0, 0).to<float>();

			cp.m_endSegment.m_size = 7;
			cp.m_endSegment.m_sizeRandom = 5;
			cp.m_endSegment.m_color = Color(0, 25, 0, 100).to<float>();
			cp.m_endSegment.m_colorRandom = Color(50, 20, 0, 0).to<float>();

			cp.m_blendMode = RenderMaterial::Screen;
			cp.m_freq = 80;
			cp.m_texture.set("graph/particles/big_greypouf", 0, 200);
			cp.m_spawnFlags = 0;
			
			pPSStream.SetParams(cp);
		}

		pPSStream.Update(timeDelta);
		pPSStream.SetPos(eCurPos);

		pPS.Update(timeDelta);
		pPS.SetPos(eCurPos);

		fTrail = ((ulCurrentTime - 750) * (1.0f / (ulDuration - 750.0f))) * 9 * (BEZIERPrecision + 2);
	}

	if(ulCurrentTime >= ulDuration)
		lightIntensityFactor = 0.f;
	else
		lightIntensityFactor = 1.f;
}

void CPoisonProjectile::Render() {
	
	if(ulCurrentTime >= ulDuration)
		return;
	
	GRenderer->SetCulling(Renderer::CullNone);
	GRenderer->SetRenderState(Renderer::DepthWrite, false);
	GRenderer->SetBlendFunc(Renderer::BlendOne, Renderer::BlendOne);
	GRenderer->SetRenderState(Renderer::AlphaBlending, true);
	
	int n = BEZIERPrecision;
	float delta = 1.0f / n;
	
	Vec3f lastpos = pathways[0];
	
	int i = 0;
	for(i = 0; i < 9; i++) {
		
		int kpprec = std::max(i - 1, 0);
		int kpsuiv = i + 1 ;
		int kpsuivsuiv = (i < (9 - 2)) ? kpsuiv + 1 : kpsuiv;
		
		for(int toto = 1; toto < n; toto++) {
			
			if(fTrail < i * n + toto) {
				break;
			}
			
			float t = toto * delta;
			
			float t2 = t * t ;
			float t3 = t2 * t ;
			float f0 = 2.f * t3 - 3.f * t2 + 1.f ;
			float f1 = -2.f * t3 + 3.f * t2 ;
			float f2 = t3 - 2.f * t2 + t ;
			float f3 = t3 - t2 ;
			
			float val = pathways[kpsuiv].x;
			float p0 = 0.5f * (val - pathways[kpprec].x);
			float p1 = 0.5f * (pathways[kpsuivsuiv].x - pathways[i].x);
			lastpos.x = f0 * pathways[i].x + f1 * val + f2 * p0 + f3 * p1;
			
			val = pathways[kpsuiv].y;
			p0 = 0.5f * (val - pathways[kpprec].y);
			p1 = 0.5f * (pathways[kpsuivsuiv].y - pathways[i].y);
			lastpos.y = f0 * pathways[i].y + f1 * val + f2 * p0 + f3 * p1;
			
			val = pathways[kpsuiv].z;
			p0 = 0.5f * (val - pathways[kpprec].z);
			p1 = 0.5f * (pathways[kpsuivsuiv].z - pathways[i].z);
			lastpos.z = f0 * pathways[i].z + f1 * val + f2 * p0 + f3 * p1;
		}
	}
	
	eCurPos = lastpos;
	
	if(fTrail >= (i * n)) {
		LaunchPoisonExplosion(lastpos);
	}
	
	GRenderer->SetCulling(Renderer::CullNone);
	GRenderer->SetRenderState(Renderer::DepthWrite, false);
	GRenderer->SetRenderState(Renderer::AlphaBlending, true);
}
