/*
 * Copyright 2011-2013 Arx Libertatis Team (see the AUTHORS file)
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

#include "gui/Credits.h"

#include <stddef.h>
#include <sstream>

#include <boost/algorithm/string/trim.hpp>

#include "core/Core.h"
#include "core/GameTime.h"
#include "core/Version.h"

#include "gui/Menu.h"
#include "gui/Text.h"
#include "gui/MenuWidgets.h"

#include "graphics/Draw.h"
#include "graphics/data/TextureContainer.h"
#include "graphics/font/Font.h"

#include "input/Input.h"

#include "io/log/Logger.h"
#include "io/resource/PakReader.h"

#include "math/Vector.h"

#include "scene/GameSound.h"

#include "util/Unicode.h"

// TODO extern globals
extern bool bFadeInOut;
extern bool bFade;
extern int iFadeAction;

struct CreditsLine {
	
	CreditsLine() {
		sPos = Vec2i_ZERO;
		fColors = Color::none;
		sourceLineNumber = -1;
	}
	
	std::string  sText;
	Color fColors;
	Vec2i sPos;
	int sourceLineNumber;
	
};

class CreditsInformations {
	
public:
	
	CreditsInformations()
		: m_scrollPosition(0.f)
		, m_lastUpdateTime(0.f)
		, m_firstVisibleLine(0)
		, m_lineHeight(-1)
		, m_windowSize(Vec2i_ZERO)
	{ }
	
	void render();
	
	void reset();
	
private:
	
	TextureContainer * m_background;
	
	std::string m_text;
	
	float m_scrollPosition;
	float m_lastUpdateTime;
	
	size_t m_firstVisibleLine;
	int m_lineHeight;
	
	Vec2i m_windowSize; // save the screen size so we know when to re-initialize the credits
	
	std::vector<CreditsLine> m_lines;
	
	bool load();
	
	bool init();
	
	void addLine(std::string & phrase, float & drawpos, int sourceLineNumber);
	
	//! Parse the credits text and compute line positions
	void layout();
	
};

static CreditsInformations g_credits;

bool CreditsInformations::load() {
	
	LogDebug("Loading credits");
	
	std::string creditsFile = "localisation/ucredits_" +  config.language + ".txt";
	
	size_t creditsSize;
	char * credits = resources->readAlloc(creditsFile, creditsSize);
	
	std::string englishCreditsFile;
	if(!credits) {
		// Fallback if there is no localised credits file
		englishCreditsFile = "localisation/ucredits_english.txt";
		credits = resources->readAlloc(englishCreditsFile, creditsSize);
	}
	
	if(!credits) {
		if(!englishCreditsFile.empty() && englishCreditsFile != creditsFile) {
			LogWarning << "Unable to read credits files " << creditsFile
			           << " and " << englishCreditsFile;
		} else {
			LogWarning << "Unable to read credits file " << creditsFile;
		}
		return false;
	}
	
	LogDebug("Loaded credits file: " << creditsFile << " of size " << creditsSize);
	
	m_text = arx_credits;
	
	m_text += "\n\n\n" + arx_copyright;
	
	m_text += "\n\n\n~ORIGINAL ARX FATALIS CREDITS:\n\n\n";
	
	char * creditsEnd = credits + creditsSize;
	m_text += util::convert<util::UTF16LE, util::UTF8>(credits, creditsEnd);
	
	LogDebug("Final credits length: " << m_text.size());
	
	free(credits);
	
	return true;
}

bool CreditsInformations::init() {
	
	if(!m_background) {
		m_background = TextureContainer::LoadUI("graph/interface/menus/menu_credits");
	}
	
	if(m_text.empty() && !load()) {
		return false;
	}
	
	if(m_lineHeight != -1 && m_windowSize == g_size.size()) {
		return true;
	}
	
	LogDebug("Layout credits");
	
	// When the screen is resized, try to keep the credits scrolled to the 'same' position
	static int anchorLine = -1;
	static float offset;
	typedef std::vector<CreditsLine>::iterator Iterator;
	if(m_lineHeight != -1 && m_firstVisibleLine < m_lines.size()) {
		// We use the first line that is still visible as our anchor
		Iterator it = m_lines.begin() + m_firstVisibleLine;
		anchorLine = it->sourceLineNumber;
		// Find the first credits line that comes from this source line
		Iterator first = it;
		while(first != m_lines.begin() && (first - 1)->sourceLineNumber == anchorLine) {
			--first;
		}
		// Find the first credits line that comes from this source line
		Iterator last = it;
		while((last + 1) != m_lines.end()
		      && (last + 1)->sourceLineNumber == anchorLine) {
			++last;
		}
		// Remember the offset from the anchor line to the current scroll position
		float pos = (first->sPos.y + last->sPos.y) * 0.5f;
		offset = (pos + m_scrollPosition) / float(m_lineHeight);
	}
	
	m_windowSize = g_size.size();
	
	layout();
	
	if(anchorLine >= 0) {
		// Find the first credits line that comes from our source anchor line
		Iterator first = m_lines.begin();
		while(first != m_lines.end()
		      && first->sourceLineNumber != anchorLine) {
			++first;
		}
		if(first != m_lines.end()) {
			// Find the last credits line that comes from our source anchor line
			Iterator last = first;
			while((last + 1) != m_lines.end()
			      && (last + 1)->sourceLineNumber == anchorLine) {
				++last;
			}
			// Restore the scroll positon using the offset to our anchor line
			float pos = (first->sPos.y + last->sPos.y) * 0.5f;
			m_scrollPosition = offset * float(m_lineHeight) - pos;
		}
	}
	
	return m_lineHeight != -1;
}

void CreditsInformations::addLine(std::string & phrase, float & drawpos,
                                  int sourceLineNumber) {
	
	CreditsLine infomations;
	infomations.sourceLineNumber = sourceLineNumber;
	
	// Determnine the type of the line
	bool isSimpleLine = false;
	if(!phrase.empty() && phrase[0] == '~') {
		// Heading
		drawpos += m_lineHeight * 0.6f;
		phrase[0] = ' ';
		infomations.fColors = Color::white;
	} else if(phrase[0] == '&') {
		// Heading continued
		infomations.fColors = Color::white;
	} else {
		// Name or text
		isSimpleLine = true;
		infomations.fColors = Color(232, 204, 143);
	}
	
	static const int Margin = 20;
	Rect linerect(g_size.width() - Margin - Margin, hFontCredits->getLineHeight());
	
	while(!phrase.empty()) {
		
		// Calculate height position
		infomations.sPos.y = static_cast<int>(drawpos);
		drawpos += m_lineHeight;
		
		// Split long lines
		long n = ARX_UNICODE_ForceFormattingInRect(hFontCredits, phrase, linerect);
		arx_assert(n >= 0 && size_t(n) < phrase.length());
		
		// Long lines are not simple
		isSimpleLine = isSimpleLine && size_t(n + 1) == phrase.length();
		
		infomations.sText = phrase.substr(0, size_t(n + 1) == phrase.length() ? n + 1 : n);
		phrase = phrase.substr(n + 1);
		
		// Center the text on the screen
		int linesize = hFontCredits->getTextSize(infomations.sText).x;
		infomations.sPos.x = (g_size.width() - linesize) / 2;
		
		if(isSimpleLine) {
			
			// Check if there is a suffix that should be styled differently
			size_t p = size_t(-1);
			for(;;) {
				p = infomations.sText.find_first_of("-(0123456789", p + 1);
				if(p == std::string::npos) {
					break;
				}
				if(p != 0 && infomations.sText[p  - 1] != ' ') {
					continue;
				}
				if(infomations.sText[p] == '(') {
					if(infomations.sText[infomations.sText.length() - 1] != ')') {
						continue;
					}
					if(infomations.sText.find_first_of('(', p + 1) != std::string::npos) {
						continue;
					}
				}
				if(infomations.sText[p] >= '0' && infomations.sText[p] < '9') {
					if(infomations.sText.find_first_not_of(".", p) == std::string::npos) {
						continue;
					}
					if(infomations.sText.find_first_not_of("0123456789.", p) != std::string::npos) {
						continue;
					}
				}
				break;
			}
			
			// Center names around the surname start
			size_t s = std::string::npos;
			if(p != std::string::npos && p > 2) {
				if(infomations.sText[p] == '-' || infomations.sText[p] == '(') {
					s = p - 1; // Skip space before the suffix
				} else {
					s = p;
				}
			} else if(p != 0) {
				s = infomations.sText.length();
			}
			if(s != std::string::npos && s != 0) {
				if(std::count(infomations.sText.begin(), infomations.sText.begin() + s, ' ') > 2) {
					s = std::string::npos; // A sentence
				} else if(infomations.sText.find_last_of(',', s - 1) != std::string::npos) {
					s = std::string::npos; // An inline list
				} else {
					s = infomations.sText.find_last_of(' ', s - 1);
				}
			}
			bool centered = false;
			if(s != std::string::npos && s!= 0) {
				int firstsize = hFontCredits->getTextSize(infomations.sText.substr(0, s)).x;
				if(firstsize < g_size.width() / 2 && linesize - firstsize < g_size.width() / 2) {
					infomations.sPos.x = g_size.width() / 2 - firstsize;
					centered = true;
				}
			}
			
			if(p != std::string::npos) {
				CreditsLine prefix = infomations;
				prefix.sText.resize(p);
				int prefixsize = hFontCredits->getTextSize(prefix.sText).x;
				if(!centered && p != 0 && prefixsize / 2 < g_size.width() / 2
					 && linesize - prefixsize / 2 < g_size.width() / 2) {
					prefix.sPos.x = (g_size.width() - prefixsize) / 2;
				}
				m_lines.push_back(prefix);
				infomations.sPos.x = prefix.sPos.x + prefixsize + 20;
				infomations.sText = infomations.sText.substr(p);
				infomations.fColors = Color::gray(0.7f);
			}
			
		}
		
		m_lines.push_back(infomations);
	}
	
}

void CreditsInformations::layout() {
	
	m_lineHeight = hFontCredits->getTextSize("aA(").y;
	
	m_lines.clear();
	
	// Retrieve the rows to display
	std::istringstream iss(m_text);
	std::string phrase;

	//Use to calculate the positions
	float drawpos = static_cast<float>(g_size.height());
	
	for(int sourceLineNumber = 0; std::getline(iss, phrase); sourceLineNumber++) {
		
		boost::trim(phrase);
		
		if(phrase.empty()) {
			// Separator line
			drawpos += 0.4f * m_lineHeight;
		} else {
			addLine(phrase, drawpos, sourceLineNumber);
		}
	}
	
	LogDebug("Credits lines: " << m_lines.size());
	
}

void CreditsInformations::render() {
	
	// Initialze the data on demand
	if(!init()) {
		LogError << "Could not initialize credits";
		reset();
		ARXmenu.currentmode = AMCM_MAIN;
		iFadeAction = -1;
		bFadeInOut = false;
		bFade = true;
	}
	
	// Draw the background
	if(m_background) {
		Rectf rect(Vec2f_ZERO, g_size.width(), g_size.height() + 1);
		UseRenderState state(render2D().noBlend());
		EERIEDrawBitmap2(rect, .999f, m_background, Color::white);
	}
	
	// Use time passed between frame to create scroll effect
	float time = arxtime.get_updated(false);
	float dtime = time - m_lastUpdateTime;
	
	static float lastKeyPressTime = 0.f;
	static float lastUserScrollTime = 0.f;
	static float scrollDirection = 1.f;
	
	float keyRepeatDelay = 256.f; // delay after key press before continuous scrolling
	float autoScrollDelay = 250.f; // ms after user input before resuming normal scrolling
	
	// Process user input
	float userScroll = 20.f * GInput->getMouseWheelDir();
	if(GInput->isKeyPressed(Keyboard::Key_UpArrow)) {
		userScroll += 0.2f * dtime;
	}
	if(GInput->isKeyPressedNowPressed(Keyboard::Key_PageUp)) {
		userScroll += 150.f;
		lastKeyPressTime = time;
	} else if(GInput->isKeyPressed(Keyboard::Key_PageUp)) {
		if(time - lastKeyPressTime > keyRepeatDelay) {
			userScroll += 0.5f * dtime;
		}
	}
	if(GInput->isKeyPressedNowPressed(Keyboard::Key_PageDown)) {
		userScroll -= 150.f;
		lastKeyPressTime = time;
	} else if(GInput->isKeyPressed(Keyboard::Key_PageDown)) {
		if(time - lastKeyPressTime > keyRepeatDelay) {
			userScroll -= 0.5f * dtime;
		}
	}
	if(GInput->isKeyPressed(Keyboard::Key_DownArrow)) {
		userScroll -= 0.2f * dtime;
	}
	m_scrollPosition += g_sizeRatio.y * userScroll;
	
	// If the user wants to scroll up, also change the automatic scroll direction …
	if(userScroll > 0.f) {
		lastUserScrollTime = time;
		scrollDirection = -1.f;
	}
	// … but restore normal scrolling after a short delay.
	if(time - lastUserScrollTime > autoScrollDelay) {
		scrollDirection = 1.f;
	}
	
	m_scrollPosition -= 0.03f * g_sizeRatio.y * dtime * scrollDirection;
	m_lastUpdateTime = time;
	
	// Don't scroll past the credits start
	m_scrollPosition = std::min(0.f, m_scrollPosition);
	
	std::vector<CreditsLine>::const_iterator it = m_lines.begin() + m_firstVisibleLine ;
	
	for(; it != m_lines.begin(); --it, --m_firstVisibleLine) {
		float yy = (it - 1)->sPos.y + m_scrollPosition;
		if (yy <= -m_lineHeight) {
			break;
		}
	}
	
	for (; it != m_lines.end(); ++it)
	{
		//Update the Y word display
		float yy = it->sPos.y + m_scrollPosition;
		
		//Display the text only if he is on the viewport
		if ((yy >= -m_lineHeight) && (yy <= g_size.height())) 
		{
			hFontCredits->draw(it->sPos.x, static_cast<int>(yy), it->sText, it->fColors);
		}
		
		if (yy <= -m_lineHeight)
		{
			++m_firstVisibleLine;
		}
		
		if ( yy >= g_size.height() )
			break ; //it's useless to continue because next phrase will not be inside the viewport
	}
	
	if(m_firstVisibleLine >= m_lines.size() && iFadeAction != AMCM_MAIN) {
		
		bFadeInOut = true;
		bFade = true;
		iFadeAction = AMCM_MAIN;
		
		ARX_MENU_LaunchAmb(AMB_MENU);
	}

	if(ProcessFadeInOut(bFadeInOut,0.1f) && iFadeAction == AMCM_MAIN) {
		reset();
		ARXmenu.currentmode = AMCM_MAIN;
		iFadeAction = -1;
		bFadeInOut = false;
		bFade = true;
	}
	
}

void CreditsInformations::reset() {
	LogDebug("Reset credits");
	m_lastUpdateTime = arxtime.get_updated(false);
	m_scrollPosition = 0;
	m_firstVisibleLine = 0;
	m_lineHeight = -1;
	m_lines.clear();
	delete m_background, m_background = NULL;
	m_text.clear();
}

void Credits::render() {
	g_credits.render();
}

void Credits::reset() {
	g_credits.reset();
}
