/*
 * Copyright (c) 2020 Eric Mertens <emertens@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <znc/Modules.h>
#include <znc/Chan.h>
#include <znc/Client.h>
#include <vector>

struct Entry {
	Entry(CString chan, CString nick, CString text, unsigned long hits = 0)
	: chanPattern(chan), nickPattern(nick), textPattern(text), hits(hits)
	{}

	CString chanPattern;
	CString nickPattern;
	CString textPattern;
	unsigned long hits;

	CString serialize() const {
		VCString parts {chanPattern, nickPattern, CString(hits), textPattern};
		return CString(" ").Join(parts.begin(), parts.end());
	}

	static Entry deserialize(CString const& line) {
		return {line.Token(0), line.Token(1), line.Token(3, true), line.Token(2).ToULong()};
	}
};

class CMyFilter: public CModule
{
	std::vector<Entry> entries;

	void Save();
	void OnAddFilter(CString const& args);
	void OnDelFilter(CString const& args);
	void OnListFilters(CString const& args);

	EModRet OnModuleUnloading(CModule *pModule, bool &bSuccess, CString &sRetMsg) override;
	EModRet OnChanNoticeMessage(CNoticeMessage &Message) override;
	EModRet OnChanTextMessage(CTextMessage &Message) override;
	bool OnLoad(const CString &sArgsi, CString &sMessage) override;

public:
 	MODCONSTRUCTOR(CMyFilter)
	{
		AddHelpCommand();
		AddCommand("add", static_cast<CModCommand::ModCmdFunc>(&CMyFilter::OnAddFilter), "<chan> <nick> <text>", "Add filter");
		AddCommand("del", static_cast<CModCommand::ModCmdFunc>(&CMyFilter::OnDelFilter), "<num>", "Delete filter");
		AddCommand("list", static_cast<CModCommand::ModCmdFunc>(&CMyFilter::OnListFilters), "", "List filters");
	}
};

void CMyFilter::Save()
{
	VCString lines;
	for (auto const& entry : entries) {
		lines.push_back(entry.serialize());
	}
	SetNV("Filters", CString("\n").Join(lines.begin(), lines.end()));
}

void CMyFilter::OnAddFilter(CString const& args)
{
	VCString result;
	size_t args_n = args.QuoteSplit(result);

	CString chan = args.Token(1);
	CString nick = args.Token(2);
	CString text = args.Token(3, true);
	entries.emplace_back(chan, nick, text);
	PutModule("Added filter");

	Save();
}

void CMyFilter::OnDelFilter(CString const& args)
{
	unsigned long i = args.Token(1).ToULong() - 1;
	if (i < entries.size()) {
		entries.erase(entries.begin() + i);
		PutModule("Filter removed.");
	} else {
		PutModule("Bad index.");
	}

	Save();
}

void CMyFilter::OnListFilters(CString const& args)
{
	if (entries.empty()) {
		PutModule("No filters");
	} else {
		CTable table;
		table.AddColumn("Channel");
		table.AddColumn("Nickname");
		table.AddColumn("Hits");
		table.AddColumn("Message");
		for (auto const& entry : entries) {
			table.AddRow();
			table.SetCell("Channel", entry.chanPattern);
			table.SetCell("Nickname", entry.nickPattern);
			table.SetCell("Hits", CString(entry.hits));
			table.SetCell("Message", entry.textPattern);
		}
		PutModule(table);
	}

	Save();
}

CModule::EModRet CMyFilter::OnModuleUnloading(CModule *pModule, bool &bSuccess, CString &sRetMsg)
{
	Save();
	bSuccess = true;
	return EModRet::CONTINUE;
}

CModule::EModRet CMyFilter::OnChanNoticeMessage(CNoticeMessage &Message)
{
	return OnChanTextMessage(Message);
}

CModule::EModRet CMyFilter::OnChanTextMessage(CTextMessage &Message)
{
	auto chan = Message.GetChan();
	auto nick = Message.GetNick();
	auto text = Message.GetText();

	for (auto &entry : entries)
	{
		if (chan->GetName().WildCmp(entry.chanPattern, CaseSensitivity::CaseInsensitive) &&
		    nick.GetNick().WildCmp(entry.nickPattern, CaseSensitivity::CaseInsensitive) &&
		    text.WildCmp(entry.textPattern, CaseSensitivity::CaseInsensitive))
		{
			entry.hits++;
			return EModRet::HALT;
		}
	}
	return EModRet::CONTINUE;
}

bool CMyFilter::OnLoad(const CString &sArgsi, CString &sMessage)
{
	if (HasNV("Filters")) {
		VCString lines;
		GetNV("Filters").Split("\n", lines);
		for (auto const& line : lines) {
			entries.push_back(Entry::deserialize(line));
		}
	}
	return true;
}

template <>
void TModInfo<CMyFilter>(CModInfo& Info) {
	Info.SetHasArgs(false);
}

NETWORKMODULEDEFS(CMyFilter, "Message filter")
