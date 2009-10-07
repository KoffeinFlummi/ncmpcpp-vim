/***************************************************************************
 *   Copyright (C) 2008-2009 by Andrzej Rybczak                            *
 *   electricityispower@gmail.com                                          *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.              *
 ***************************************************************************/

#include "tiny_tag_editor.h"

#ifdef HAVE_TAGLIB_H

// taglib includes
#include "mpegfile.h"
#include "vorbisfile.h"
#include "flacfile.h"

#include "browser.h"
#include "charset.h"
#include "display.h"
#include "global.h"
#include "info.h"
#include "playlist.h"
#include "search_engine.h"
#include "tag_editor.h"

using Global::MainHeight;
using Global::MainStartY;
using Global::myOldScreen;
using Global::myScreen;
using Global::wFooter;

TinyTagEditor *myTinyTagEditor = new TinyTagEditor;

void TinyTagEditor::Init()
{
	w = new Menu<Buffer>(0, MainStartY, COLS, MainHeight, "", Config.main_color, brNone);
	w->HighlightColor(Config.main_highlight_color);
	w->SetTimeout(ncmpcpp_window_timeout);
	w->CyclicScrolling(Config.use_cyclic_scrolling);
	w->SetItemDisplayer(Display::Generic);
	isInitialized = 1;
}

void TinyTagEditor::Resize()
{
	w->Resize(COLS, MainHeight);
	w->MoveTo(0, MainStartY);
	hasToBeResized = 0;
}

void TinyTagEditor::SwitchTo()
{
	if (itsEdited.isStream())
	{
		ShowMessage("Cannot edit streams!");
	}
	else if (GetTags())
	{
		if (hasToBeResized)
			Resize();
		myOldScreen = myScreen;
		myScreen = this;
		Global::RedrawHeader = 1;
	}
	else
	{
		std::string message = "Cannot read file '";
		if (itsEdited.isFromDB())
			message += Config.mpd_music_dir;
		message += itsEdited.GetFile();
		message += "'!";
		ShowMessage("%s", message.c_str());
	}
}

std::basic_string<my_char_t> TinyTagEditor::Title()
{
	return U("Tiny tag editor");
}

void TinyTagEditor::EnterPressed()
{
	size_t option = w->Choice();
	MPD::Song &s = itsEdited;
	
	if (option < 20) // separator after filename
		w->at(option).Clear();
	
	LockStatusbar();
	if (option < 17) // separator after comment
	{
		size_t pos = option-8;
		Statusbar() << fmtBold << Info::Tags[pos].Name << ": " << fmtBoldEnd;
		s.SetTags(Info::Tags[pos].Set, wFooter->GetString(s.GetTags(Info::Tags[pos].Get)));
		w->at(option) << fmtBold << Info::Tags[pos].Name << ':' << fmtBoldEnd << ' ';
		ShowTag(w->at(option), s.GetTags(Info::Tags[pos].Get));
	}
	else if (option == 19)
	{
		Statusbar() << fmtBold << "Filename: " << fmtBoldEnd;
		std::string filename = s.GetNewName().empty() ? s.GetName() : s.GetNewName();
		size_t dot = filename.rfind(".");
		std::string extension = filename.substr(dot);
		filename = filename.substr(0, dot);
		std::string new_name = wFooter->GetString(filename);
		s.SetNewName(new_name + extension);
		w->at(option) << fmtBold << "Filename:" << fmtBoldEnd << ' ' << (s.GetNewName().empty() ? s.GetName() : s.GetNewName());
	}
	UnlockStatusbar();
	
	if (option == 21)
	{
		ShowMessage("Updating tags...");
		if (TagEditor::WriteTags(s))
		{
			ShowMessage("Tags updated!");
			if (s.isFromDB())
			{
				Mpd.UpdateDirectory(locale_to_utf_cpy(s.GetDirectory()));
				if (myOldScreen == mySearcher) // songs from search engine are not updated automatically
					*mySearcher->Main()->Current().second = s;
			}
			else
			{
				if (myOldScreen == myPlaylist)
					myPlaylist->Items->Current() = s;
				else if (myOldScreen == myBrowser)
					*myBrowser->Main()->Current().song = s;
			}
		}
		else
			ShowMessage("Error writing tags!");
	}
	if (option > 20)
		myOldScreen->SwitchTo();
}

void TinyTagEditor::MouseButtonPressed(MEVENT me)
{
	if (w->Empty() || !w->hasCoords(me.x, me.y) || size_t(me.y) >= w->Size())
		return;
	if (me.bstate & (BUTTON1_PRESSED | BUTTON3_PRESSED))
	{
		if (!w->Goto(me.y))
			return;
		if (me.bstate & BUTTON3_PRESSED)
		{
			w->Refresh();
			EnterPressed();
		}
	}
	else
		Screen< Menu<Buffer> >::MouseButtonPressed(me);
}

bool TinyTagEditor::SetEdited(MPD::Song *s)
{
	if (!s)
		return false;
	itsEdited = *s;
	return true;
}

bool TinyTagEditor::GetTags()
{
	MPD::Song &s = itsEdited;
	
	std::string path_to_file;
	if (s.isFromDB())
		path_to_file += Config.mpd_music_dir;
	path_to_file += s.GetFile();
	locale_to_utf(path_to_file);
	
	TagLib::FileRef f(path_to_file.c_str());
	if (f.isNull())
		return false;
	s.SetComment(f.tag()->comment().to8Bit(1));
	
	std::string ext = s.GetFile();
	ext = ext.substr(ext.rfind(".")+1);
	ToLower(ext);
	
	if (!isInitialized)
		Init();
	
	w->Clear();
	w->Reset();
	
	w->ResizeList(23);
	
	for (size_t i = 0; i < 7; ++i)
		w->Static(i, 1);
	
	w->IntoSeparator(7);
	w->IntoSeparator(18);
	w->IntoSeparator(20);
	
	if (!extendedTagsSupported(f.file()))
		for (size_t i = 14; i <= 16; ++i)
			w->Static(i, 1);
	
	w->Highlight(8);
	
	w->at(0) << fmtBold << Config.color1 << "Song name: " << fmtBoldEnd << Config.color2 << s.GetName() << clEnd;
	w->at(1) << fmtBold << Config.color1 << "Location in DB: " << fmtBoldEnd << Config.color2;
	ShowTag(w->at(1), s.GetDirectory());
	w->at(1) << clEnd;
	w->at(3) << fmtBold << Config.color1 << "Length: " << fmtBoldEnd << Config.color2 << s.GetLength() << clEnd;
	w->at(4) << fmtBold << Config.color1 << "Bitrate: " << fmtBoldEnd << Config.color2 << f.audioProperties()->bitrate() << " kbps" << clEnd;
	w->at(5) << fmtBold << Config.color1 << "Sample rate: " << fmtBoldEnd << Config.color2 << f.audioProperties()->sampleRate() << " Hz" << clEnd;
	w->at(6) << fmtBold << Config.color1 << "Channels: " << fmtBoldEnd << Config.color2 << (f.audioProperties()->channels() == 1 ? "Mono" : "Stereo") << clDefault;
	
	unsigned pos = 8;
	for (const Info::Metadata *m = Info::Tags; m->Name; ++m, ++pos)
	{
		w->at(pos) << fmtBold << m->Name << ":" << fmtBoldEnd << ' ';
		ShowTag(w->at(pos), s.GetTags(m->Get));
	}
	
	w->at(19) << fmtBold << "Filename:" << fmtBoldEnd << ' ' << s.GetName();
	
	w->at(21) << "Save";
	w->at(22) << "Cancel";
	return true;
}

bool TinyTagEditor::extendedTagsSupported(TagLib::File *f)
{
	return	dynamic_cast<TagLib::MPEG::File *>(f)
	||	dynamic_cast<TagLib::Ogg::Vorbis::File *>(f)
	||	dynamic_cast<TagLib::FLAC::File *>(f);
}

#endif // HAVE_TAGLIB_H
