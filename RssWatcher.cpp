#include "stdafx.h"
#include "util.h"
#include "Rss.h"
#include "RssWatcher.h"
#include "Newzflow.h"
#include "Settings.h"

#ifdef _DEBUG
#define new DEBUG_CLIENTBLOCK
#endif

// CRssWatcher
//////////////////////////////////////////////////////////////////////////

CRssWatcher::CRssWatcher()
: CThreadImpl<CRssWatcher>(CREATE_SUSPENDED)
, shutDown(TRUE, FALSE)
, refresh(FALSE, FALSE)
{
	Resume();
}

DWORD CRssWatcher::Run()
{
	int delay = 0;
	downloader.Init();

	for(;;) {
		if(CNewzflow::Instance()->IsShuttingDown() || WaitForSingleObject(shutDown, 0) == WAIT_OBJECT_0)
			break;

		if(delay > 0) {
			HANDLE objs[] = { shutDown, refresh };
			WaitForMultipleObjects(2, objs, FALSE, delay * 1000);
			delay = 0;
			continue;
		}

		// TODO: add column names to Reader/Statement
		sq3::Statement st(CNewzflow::Instance()->database, _T("SELECT rowid, url, title FROM RssFeeds WHERE last_update ISNULL OR (strftime('%s', 'now') - strftime('%s', last_update) / 60) >= update_interval"));
		sq3::Reader reader = st.ExecuteReader();
		while(reader.Step() == SQLITE_ROW) {
			int id; reader.GetInt(0, id);
			CString sUrl; reader.GetString(1, sUrl);
			CString sTitle; reader.GetString(2, sTitle);
			CString s; s.Format(_T("ProcessFeed(%s (%d), %s)\n"), sTitle, id, sUrl); Util::Print(s);
			ProcessFeed(id, sUrl);
			Util::GetMainWindow().PostMessage(Util::MSG_RSSFEED_UPDATED);
		}

		delay = 60;
	}

	return 0;
}

void CRssWatcher::ProcessFeed(int id, const CString& sUrl)
{
	CString rssFile = CNewzflow::Instance()->settings->GetAppDataDir() + _T("temp.rss");
	if(downloader.Download(sUrl, rssFile, CString(), NULL)) {
		CRss rss;
		if(rss.Parse(rssFile)) {
			sq3::Transaction transaction(CNewzflow::Instance()->database);
			for(size_t i = 0; i < rss.items.GetCount(); i++) {
				CRssItem* item = rss.items[i];
				sq3::Statement st(CNewzflow::Instance()->database, _T("INSERT OR IGNORE INTO RssItems (feed, title, link, length, description, category, date) VALUES (?, ?, ?, ?, ?, ?, julianday(?))"));
				st.Bind(0, id);
				st.Bind(1, item->title);
				if(item->enclosure) {
					st.Bind(2, item->enclosure->url);
					st.Bind(3, item->enclosure->length);
				} else {
					st.Bind(2, item->link);
					st.Bind(3, 0);
				}
				st.Bind(4, item->description);
				st.Bind(5, item->category);
				st.Bind(6, item->pubDate.Format(_T("%Y-%m-%d %H:%M:%S")));
				if(st.ExecuteNonQuery() != SQLITE_OK) {
					TRACE(_T("DB error: %s\n"), CNewzflow::Instance()->database.GetErrorMessage());
				}
			}
			// set update interval
			if(rss.ttl > 0) {
				sq3::Statement st(CNewzflow::Instance()->database, _T("UPDATE RssFeeds SET update_interval = ? WHERE rowid = ?"));
				st.Bind(0, rss.ttl);
				st.Bind(1, id);
				if(st.ExecuteNonQuery() != SQLITE_OK) {
					TRACE(_T("DB error: %s\n"), CNewzflow::Instance()->database.GetErrorMessage());
				}
			}
			// set last update
			{
				sq3::Statement st(CNewzflow::Instance()->database, _T("UPDATE RssFeeds SET last_update = julianday('now') WHERE rowid = ?"));
				st.Bind(0, id);
				if(st.ExecuteNonQuery() != SQLITE_OK) {
					TRACE(_T("DB error: %s\n"), CNewzflow::Instance()->database.GetErrorMessage());
				}
			}
			// set title from feed if previously empty
			{
				sq3::Statement st(CNewzflow::Instance()->database, _T("UPDATE RssFeeds SET title = ? WHERE rowid = ? AND title ISNULL"));
				st.Bind(0, rss.title);
				st.Bind(1, id);
				if(st.ExecuteNonQuery() != SQLITE_OK) {
					TRACE(_T("DB error: %s\n"), CNewzflow::Instance()->database.GetErrorMessage());
				}
			}
			transaction.Commit();
		}
	}
}