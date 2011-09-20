// This file is part of foo_androidsync.

// foo_androidsync is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// foo_androidsync is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
   
// You should have received a copy of the GNU Lesser General Public License
// along with foo_androidsync.  If not, see <http://www.gnu.org/licenses/>.

#include "stdafx.h"
#include "resource.h"

using namespace pfc::stringcvt;

// = Declarations =

const pfc::string8 APP_NAME = "Android Sync";

typedef vector<string> filename_list;

void androidsync_basename( pfc::string8 &, pfc::string8 & );
void androidsync_remote( pfc::string8 &, pfc::string8 & );
void androidsync_do_sync();

DECLARE_COMPONENT_VERSION(
   "Android Sync Component",
   "0.8.10",
   "Select the playlists you would like to synchronize from the \"Synchronize "
   "Playlists\" preference panel and then select \"Android Sync\" -> \""
   "Synchronize Playlists\" from the file menu to perform synchronization."
);

// This will prevent users from renaming your component around (important for 
// proper troubleshooter behaviors) or loading multiple instances of it.
VALIDATE_COMPONENT_FILENAME( "foo_androidsync.dll" );

static const GUID g_mainmenu_group_id = {
   // {ACE8A3BE-8E51-4FBA-BB22-4560643BC346}
   0xace8a3be, 0x8e51, 0x4fba, 
   { 0xbb, 0x22, 0x45, 0x60, 0x64, 0x3b, 0xc3, 0x46 }
};

// These GUIDs identify the variables within our component's configuration file.
static const GUID guid_cfg_selectedplaylists = {
   // {C1D68A14-464C-4B44-8DDE-A1723AFD881F}
   0xc1d68a14, 0x464c, 0x4b44, 
   { 0x8d, 0xde, 0xa1, 0x72, 0x3a, 0xfd, 0x88, 0x1f } 
};

static const GUID guid_cfg_targetpath = {
   // {A0EF91CA-E192-4F5A-9DA3-0B5F300E26C8}
   0xa0ef91ca, 0xe192, 0x4f5a, { 0x9d, 0xa3, 0xb, 0x5f, 0x30, 0xe, 0x26, 0xc8 }
};

static cfg_objList<pfc::string8> cfg_selectedplaylists( guid_cfg_selectedplaylists );
static cfg_string cfg_targetpath( guid_cfg_targetpath, "" );



/* = Main Menu Additions = */

static mainmenu_group_popup_factory g_mainmenu_group( 
   g_mainmenu_group_id, 
   mainmenu_groups::file, 
   mainmenu_commands::sort_priority_dontcare, 
   "Android Sync"
);

class mainmenu_commands_androidsync : public mainmenu_commands {
public:
	enum {
		cmd_sync = 0,
		cmd_total
	};

	t_uint32 get_command_count() {
		return cmd_total;
	}

	GUID get_command( t_uint32 p_index ) {
		static const GUID guid_sync = {
         // {F24A7301-5B86-420B-A1E8-B1EE83CEF4DD}
         0xf24a7301, 0x5b86, 0x420b, 
         { 0xa1, 0xe8, 0xb1, 0xee, 0x83, 0xce, 0xf4, 0xdd }

      };

		switch(p_index) {
			case cmd_sync: 
            return guid_sync;

			default: 
            // This should never happen unless somebody called us with invalid 
            // parameters; bail.
            uBugCheck();
		}
	}

	void get_name( t_uint32 p_index, pfc::string_base &p_out ) {
		switch( p_index ) {
			case cmd_sync: 
            p_out = "Synchronize Playlists";
            break;

			default: 
            // This should never happen unless somebody called us with invalid 
            // parameters; bail.
            uBugCheck();
		}
	}

	bool get_description( t_uint32 p_index,pfc::string_base &p_out ) {
		switch( p_index ) {
			case cmd_sync:
            p_out = "Synchronize the playlists selected in preferences to "
               "the connected android device";
            return true;

			default: 
            // This should never happen unless somebody called us with invalid 
            // parameters; bail.
            uBugCheck();
		}
	}

	GUID get_parent() {
		return g_mainmenu_group_id;
	}

	void execute( t_uint32 p_index, service_ptr_t<service_base> p_callback ) {
		switch( p_index ) {
			case cmd_sync:
				androidsync_do_sync();
				break;

			default:
            // This should never happen unless somebody called us with invalid 
            // parameters; bail.
				uBugCheck();
		}
	}
};

static mainmenu_commands_factory_t<mainmenu_commands_androidsync> 
   g_mainmenu_commands_androidsync_factory;



/* = Preferences = */

class CAndroidSyncPrefs : public CDialogImpl<CAndroidSyncPrefs>, 
public preferences_page_instance {
public:
	// Constructor, invoked by preferences_page_impl helpers. Don't do Create()
   // in here, preferences_page_impl does this for us.
	CAndroidSyncPrefs( preferences_page_callback::ptr callback ) : 
      m_callback( callback ) {}

	// Note that we don't bother doing anything regarding destruction of our 
   // class. The host ensures that our dialog is destroyed first, then the 
   // last reference to our preferences_page_instance object is released, 
   // causing our object to be deleted.

	// Dialog Resource ID
	enum { IDD = IDD_ANDROIDSYNCPREFS };

   // preferences_page_instance methods (not all of them - get_wnd() is 
   // supplied by preferences_page_impl helpers).
	t_uint32 get_state() {
	   t_uint32 state = preferences_state::resettable;
	
      if( has_changed() ) {
         state |= preferences_state::changed;
      }

	   return state;
   }

   void apply() {
	   // cfg_selectedplaylists = GetDlgItemInt(IDC_SELECTEDPLAYLISTS, NULL, FALSE);
	   // cfg_targetpath = GetDlgItemInt(IDC_TARGETPATH, NULL, FALSE);
      int item_count,
         i,
         item_index_iter,
         * item_indexes;
      pfc::string8 item_iter_8;
      LPTSTR targetpath_wide = new TCHAR[MAX_PATH];

      // Clear the existing list of selected playlists before we begin.
      cfg_selectedplaylists.remove_all();

      // Get the currently selected playlists from the listbox.
      item_count = SendDlgItemMessage(
         IDC_SELECTEDPLAYLISTS,
         LB_GETSELCOUNT,
         NULL, NULL
      );
      item_indexes = new int[item_count];
      SendDlgItemMessage(
         IDC_SELECTEDPLAYLISTS,
         LB_GETSELITEMS,
         (WPARAM)item_count,
         (LPARAM)item_indexes
      );

      // Save the currently selected list of playlists to the configuration
      // variable.
      for( i = 0 ; i < item_count ; i++ ) {
         item_index_iter = item_indexes[i];
      
         // XXX: This just seems to return blank strings/crashes/heartache.
         /* item_iter_wide = (LPCTSTR)SendDlgItemMessage(
            IDC_SELECTEDPLAYLISTS,
            LB_GETITEMDATA,
            (WPARAM)item_index_iter,K
            0
         ); */
      
         // This is kind of cheating since we're not pulling it from the listbox
         // itself, but it will probably work for the time being since we're 
         // pulling from the same source it gets the names from.
         static_api_ptr_t<playlist_manager>()->playlist_get_name( 
            item_index_iter, item_iter_8 
         );

         cfg_selectedplaylists.add_item( item_iter_8 );
      }
      delete item_indexes;

      // Save the current target path.
      GetDlgItemText( IDC_TARGETPATH, targetpath_wide, MAX_PATH );
      cfg_targetpath.set_string(
         pfc::stringcvt::string_utf8_from_wide( targetpath_wide )
      );

      // Our dialog content has not changed but the flags have - our currently 
      // shown values now match the settings so the apply button can be disabled.
	   on_change();
   }

   void reset() {
      SendDlgItemMessage(
         IDC_SELECTEDPLAYLISTS,
         LB_SETSEL,
         (WPARAM)FALSE,
         (LPARAM)-1
      );
      SetDlgItemText( IDC_TARGETPATH, _T( "" ) );

      on_change();
   }

	// WTL Message Map
	BEGIN_MSG_MAP( CAndroidSyncPrefs )
		MSG_WM_INITDIALOG( on_init_dialog )
		COMMAND_HANDLER_EX( IDC_SELECTEDPLAYLISTS, LBN_SELCHANGE, on_sel_change )
		COMMAND_HANDLER_EX( IDC_TARGETPATH, EN_CHANGE, on_edit_change )
	END_MSG_MAP()

private:

   BOOL on_init_dialog(CWindow, LPARAM) {
      t_size plist_id_iter, // ID of the currently iterated playlist.
         cpare_id_iter; // ID of the string plist_iter is compared against.
      pfc::string8 cpare_iter,
         playlist_name;

      // Populate the list box with all available playlists.
      for( 
         plist_id_iter = 0;
         plist_id_iter < static_api_ptr_t<playlist_manager>()->get_playlist_count();
         plist_id_iter++
      ) {
   
         static_api_ptr_t<playlist_manager>()->playlist_get_name( 
            plist_id_iter, playlist_name 
         );

         // Modern Foobar plugin projects have to be unicode-enabled, which causes
         // all Windows API functions to require wide character inputs. However,
         // the playlist manager function above uses the proprietary PFC string 
         // class which is not wide. Therefore, the string must be converted using 
         // the stringcvt helper function. This information was found conveniently 
         // in the cellar of the Foobar documentation, with the aid of a 
         // flashlight, in the bottom of a locked filing cabinet stuck in a disused
         // lavatory with a sign on the door saying, "Beware of the Leopard".
         SendDlgItemMessage(
            IDC_SELECTEDPLAYLISTS,
            LB_ADDSTRING,
            (WPARAM)0,
            (LPARAM)((LPCTSTR)pfc::stringcvt::string_wide_from_utf8( playlist_name ))
         );

         // Select user-selected playlists in the list box.
         for( 
            cpare_id_iter = 0;
            cpare_id_iter < cfg_selectedplaylists.get_count();
            cpare_id_iter++
         ) {
            cpare_iter = cfg_selectedplaylists.get_item( cpare_id_iter );

            if( 0 == pfc::comparator_strcmp::compare( cpare_iter, playlist_name ) ) {
               SendDlgItemMessage(
                  IDC_SELECTEDPLAYLISTS,
                  LB_SETSEL,
                  (WPARAM)TRUE,
                  (LPARAM)plist_id_iter
               );
            }
         }
      }

      // Set the target path control to the user-selected value.
      SetDlgItemText(
         IDC_TARGETPATH, 
         pfc::stringcvt::string_wide_from_utf8( cfg_targetpath )
      );
   
	   return FALSE;
   }

   void on_edit_change(UINT, int, CWindow) {
	   on_change();
   }

   void on_sel_change(UINT, int, CWindow) {
      on_change();
   }

   void on_change() {
	   // Tell the host that our state has changed to enable/disable the apply 
      // button appropriately.
	   m_callback->on_state_changed();
   }

   // Returns whether our dialog content is different from the current 
   // configuration (whether the apply button should be enabled or not).
   bool has_changed() {
	   /* LPCTSTR targetpath_text;
      bool changed = false;
      int item_count,
         i,
         item_index_iter,
         * item_indexes;
      pfc::string8 item_iter_8;

      // Get the currently selected playlists from the listbox.
      item_count = SendDlgItemMessage(
         IDC_SELECTEDPLAYLISTS,
         LB_GETSELCOUNT,
         NULL, NULL
      );
      item_indexes = new int[item_count];
      SendDlgItemMessage(
         IDC_SELECTEDPLAYLISTS,
         LB_GETSELITEMS,
         (WPARAM)item_count,
         (LPARAM)item_indexes
      ); */

      // See if the playlist selections have changed.
      /* for( i = 0 ; i < item_count ; i++ ) {
         item_index_iter = item_indexes[i];

         // This is kind of cheating since we're not pulling it from the listbox
         // itself, but it will probably work for the time being since we're 
         // pulling from the same source it gets the names from.
         static_api_ptr_t<playlist_manager>()->playlist_get_name( 
            item_index_iter, item_iter_8 
         );

         if( 0 != cfg_selectedplaylists.find_item( item_iter_8 ) ) {
         
         }
      }
      delete item_indexes; */

      // See if the target path changed.
      /* GetDlgItemText( IDC_SELECTEDPLAYLISTS, targetpath_text,  );
      if( 0 != pfc::comparator_strcmp::compare( targetpath_text, cfg_targetpath ) ) {
         changed = true;
      } */

      // return changed;
      // XXX
      return true;
   }
   
   const preferences_page_callback::ptr m_callback;
};

class preferences_page_androidsync : 
public preferences_page_impl<CAndroidSyncPrefs> {
public:
   const char * get_name() { return APP_NAME; }

	GUID get_guid() {
		static const GUID guid = {
         // {77329F7B-F28A-404A-8A0B-5F50426F4211}
         0x77329f7b, 0xf28a, 0x404a, 
         { 0x8a, 0xb, 0x5f, 0x50, 0x42, 0x6f, 0x42, 0x11 }
      };

      return guid;
	}

	GUID get_parent_guid() { return guid_tools; }
};

static preferences_page_factory_t<preferences_page_androidsync>
   g_preferences_page_androidsync_factory;



/* = Synchronization = */

// Figure out the base name of the given item. 
void androidsync_basename( pfc::string8 &path_in, pfc::string8 &basename_out ) {
   const char* item_basename_start;

   // Setup.
   basename_out.reset();

   // Base name is the file name minus the path. Maybe there's a better way to 
   // do this?
   item_basename_start = strrchr( path_in, '\\' );
   basename_out << 
      (item_basename_start ? item_basename_start + 1 : path_in);
}

string basename( string path_in )
{
   size_t end = path_in.find_last_not_of( "\\" );
   if( end == path_in.npos )
      return "";

   size_t start = path_in.find_last_of( "\\", end );
   if( start == path_in.npos )
      start = 0;
   else
      ++start;

    return path_in.substr( start, end-start+1 );
}

string androidsync_basename( pfc::string8 &path_in ) {
   pfc::string8 path_out;
   androidsync_basename(path_in, path_out);
   return path_out.get_ptr();
}

// Figure out the destination/target path for the given item. 
void androidsync_remote( pfc::string8 &path_in, pfc::string8 &remote_out ) {
   pfc::string8 item_basename;
   
   // Setup.
   androidsync_basename( path_in, item_basename );
   remote_out.reset();

   // Remote name is the target path plus the base name.
   remote_out << cfg_targetpath;
   if( !cfg_targetpath.ends_with( '\\' ) ) {
      // There's no trailing backslash, so add one.
      remote_out.add_char( '\\' );
   }
   remote_out << item_basename;
}

string androidsync_remote( pfc::string8 &path_in ) {
   pfc::string8 path_out;
   androidsync_remote(path_in, path_out);
   return path_out.get_ptr();
}

string androidsync_remote( string path_in ) {
   string item_basename = basename( path_in );

   // Remote name is the target path plus the base name.
   string result(cfg_targetpath.get_ptr());
   if( !cfg_targetpath.ends_with( '\\' ) ) {
      // There's no trailing backslash, so add one.
      result.append("\\");
   }
   result.append(item_basename);
   return result;
}

static bool SimpleWaitForSingleObject( HANDLE h, DWORD ms )
{
   assert( h != NULL );

   DWORD ret = WaitForSingleObject( h, ms );
   switch( ret )
   {
   case WAIT_OBJECT_0:
      return true;

   case WAIT_TIMEOUT:
      return false;

   case WAIT_ABANDONED:
      /* The docs aren't particular about what this does, but it should never happen. */
      abort();

   case WAIT_FAILED:
      abort();
   }

   return false;
}

bool FileReadable(wstring path)
{
   DWORD result = GetFileAttributes(path.c_str());
   if(result == 0xFFFFFFFF)
      return false;
   if(result & FILE_ATTRIBUTE_DIRECTORY)
      return false;

   return true;
}

class WindowsFile
{
public:
    WindowsFile(HANDLE handle_)
    {
       handle = handle_;
    }

    ~WindowsFile()
    {
       Close();
    }

    void Close()
    {
        if(handle != INVALID_HANDLE_VALUE)
            CloseHandle(handle);
        handle = INVALID_HANDLE_VALUE;

    }

    HANDLE GetHandle() { return handle; }

private:
    HANDLE handle;
};

string vssprintf(const char *szFormat, va_list va)
{
   string sStr;

   char *pBuf = NULL;
   int iChars = 1;
   int iUsed = 0;
   int iTry = 0;

   do
   {
      iChars += iTry * 2048;
      pBuf = (char*) _alloca(sizeof(char)*iChars);
      iUsed = vsnprintf(pBuf, iChars-1, szFormat, va);
      ++iTry;
   } while(iUsed < 0);

   // assign whatever we managed to format
   sStr.assign(pBuf, iUsed);
   return sStr;
}

string ssprintf(const char *fmt, ...)
{
   va_list va;
   va_start(va, fmt);
   return vssprintf(fmt, va);
}

string GetWindowsError(int iErr)
{
   char szBuf[1024] = "";
   FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, 0, iErr, 0, szBuf, sizeof(szBuf), NULL);

   // For some reason, these messages are returned with a trailing \r\n.  Remove it.
   if(*szBuf && szBuf[strlen(szBuf)-1] == '\n')
      szBuf[strlen(szBuf)-1] = 0;
   if(*szBuf && szBuf[strlen(szBuf)-1] == '\r')
      szBuf[strlen(szBuf)-1] = 0;

   return szBuf;
}

LARGE_INTEGER FileTimeToLargeInt(FILETIME time)
{
   LARGE_INTEGER ret;
   ret.LowPart = time.dwLowDateTime;
   ret.HighPart = time.dwHighDateTime;
   return ret;
}

inline long long llabs(long long i)
{
   return i >= 0? i: -i;
}

// Return true if both from and to exist, and have the same write time and
// file size.
bool FileExists(wstring from, wstring to)
{
   WindowsFile fromFile(CreateFileW(from.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL));
   WindowsFile toFile(CreateFileW(to.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL));
   if(fromFile.GetHandle() == INVALID_HANDLE_VALUE || toFile.GetHandle() == INVALID_HANDLE_VALUE)
      return false;

   LARGE_INTEGER fromSize = {0}, toSize = {0};
   GetFileSizeEx(fromFile.GetHandle(), &fromSize);
   GetFileSizeEx(toFile.GetHandle(), &toSize);
   if(fromSize.QuadPart != toSize.QuadPart)
      return false;

   FILETIME fromTime, toTime;
   GetFileTime(fromFile.GetHandle(), NULL, NULL, &fromTime);
   GetFileTime(toFile.GetHandle(), NULL, NULL, &toTime);

   // FAT32 modification times have a resolution of two seconds, so we need to allow some
   // variation in the timestamps.
   LARGE_INTEGER fromTimeInt = FileTimeToLargeInt(fromTime);
   LARGE_INTEGER toTimeInt = FileTimeToLargeInt(toTime);
   if(llabs(fromTimeInt.QuadPart - toTimeInt.QuadPart) > 50000000) // 5 seconds
       return false;

   return true;
}

class Sync
{
public:
   Sync()
   {
      cancelled = false;
      files_finished = 0;
   }

   // Return progress, on a scale of [0,1000].
   int get_progress() const
   {
      insync(&lock);

      int total_files = sourceToDest.size() + playlists.size();
      if(total_files == 0)
         return 0;

      return (files_finished * 1000) / total_files;
   }

   void init_sync();
   void perform_sync();

   void cancel() { cancelled = true; }

   string get_log() const { return log; }

private:
   void purge_files();
   void write_playlists();
   void copy_files();
   void remove_files();

   void make_item_list(t_size playlist_id_in);
   void make_playlists(t_size playlist_id_in);

   // Files and playlists to copy; initialized by init_sync:
   map<wstring, wstring> sourceToDest;
   map<string, filename_list> playlists;

   vector<wstring> all_playlist_items;

   int files_finished;

   volatile bool cancelled;
   string log;
   mutable critical_section lock;
};


void Sync::make_item_list(t_size playlist_id_in)
{
   // Build the source path list.
   pfc::list_t<metadb_handle_ptr> playlist_items;
   static_api_ptr_t<playlist_manager>()->playlist_get_all_items(playlist_id_in, playlist_items);

   for(t_size item_iter_id = 0; item_iter_id < playlist_items.get_count(); item_iter_id++)
   {
      const metadb_handle *entry = playlist_items.get_item( item_iter_id ).get_ptr();
      pfc::string8 item_iter;
      filesystem::g_get_display_path(entry->get_path(), item_iter);

      wstring src_path = pfc::stringcvt::string_wide_from_utf8(item_iter);
      wstring dst_path = pfc::stringcvt::string_wide_from_utf8(androidsync_remote(item_iter).c_str());

      // Record the filename.  Don't access any files until we're in the thread, since it may take a while.
      sourceToDest[src_path] = dst_path;
   }
}

bool CopyFileInner(WindowsFile &fromFile, WindowsFile &toFile, volatile bool *cancelled)
{
   char buf[1024*64];
   DWORD got;
   while(!*cancelled)
   {
      if(!ReadFile(fromFile.GetHandle(), buf, sizeof(buf), &got, NULL))
         return false;

      if(got == 0)
         break;

      DWORD wrote;
      if(!WriteFile(toFile.GetHandle(), buf, got, &wrote, NULL))
         return false;
   }
   return true;
}

bool CopyFile(wstring from, wstring to, string &error, volatile bool *cancelled)
{
   WindowsFile fromFile(CreateFileW(from.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL));
   if(fromFile.GetHandle() == INVALID_HANDLE_VALUE)
   {
      error = GetWindowsError(GetLastError());
      return false;
   }

   WindowsFile toFile(CreateFileW(to.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL));
   if(toFile.GetHandle() == INVALID_HANDLE_VALUE)
   {
      error = GetWindowsError(GetLastError());
      return false;
   }

   if(!CopyFileInner(fromFile, toFile, cancelled))
   {
      // Stash the error, since the code below will clear it.
      int errorNumber = GetLastError();

      // If copying the file fails, delete the incomplete file.
      toFile.Close();
      DeleteFileW(to.c_str());

      error = GetWindowsError(errorNumber);
      return false;
   }

   // Copy file times to the new file.
   FILETIME creation, access, write;
   GetFileTime(fromFile.GetHandle(), &creation, &access, &write);
   SetFileTime(toFile.GetHandle(), &creation, &access, &write);

   return true;
}

// Remove files that won't be written, so the progress meter is accurate.
// This is done from the thread, so it doesn't block the UI.
void Sync::purge_files()
{
   map<wstring, wstring> newSourceToDest;
   for(map<wstring, wstring>::const_iterator it = sourceToDest.begin();
       it != sourceToDest.end(); ++it)
   {
      wstring src_path = it->first;
      wstring dst_path = it->second;

      if(!FileReadable(src_path))
         continue;

      // Only add the file to the list if it doesn't already exist at the 
      // destination.
      if(FileExists(src_path, dst_path))
          continue;

      newSourceToDest[src_path] = dst_path;
   }

   lock.enter();
   sourceToDest = newSourceToDest;
   lock.leave();
}

// Copy all of the files on a given playlist to the target directory as 
// specified in the configuration options.
void Sync::copy_files()
{
   for(map<wstring, wstring>::const_iterator it = sourceToDest.begin();
       it != sourceToDest.end(); ++it, ++files_finished)
   {
      if(cancelled)
         return;

      wstring src_path = it->first;
      wstring dst_path = it->second;

      // Add this file to the list of all items.
      all_playlist_items.push_back( src_path );

      if(!FileReadable(src_path)) {
         continue;
      }

      // Only add the file to the list if it doesn't already exist at the 
      // destination.
      if(FileExists(src_path, dst_path))
          continue;

      string error;
      if(!CopyFile(src_path, dst_path, error, &cancelled))
         log += ssprintf("Error copying file %s: %s\n", string_utf8_from_wide(src_path.c_str()).get_ptr(), error.c_str());

      // If the operation was cancelled, CopyFile may leave an incomplete file behind;
      // delete it.
      if(cancelled)
      {
         // log += ssprintf("Deleting %s (aborted by user)", string_utf8_from_wide(dst_path.c_str()).get_ptr());
         DeleteFileW(dst_path.c_str());
      }
   }
}

void Sync::write_playlists()
{
   for(map<string, filename_list>::const_iterator it = playlists.begin();
       it != playlists.end(); ++it, ++files_finished)
   {
      if(cancelled)
         return;

      string playlist_name = it->first;
      const vector<string> &filenames = it->second;

      wstring playlist_path_remote = string_wide_from_utf8(androidsync_remote(playlist_name).c_str());

      // The playlist is a copied file too!
      all_playlist_items.push_back( playlist_path_remote );

      // Open the file.
      FILE *playlist = _wfopen(playlist_path_remote.c_str(), L"w" );
      if( playlist == NULL ) {
         log += ssprintf("Error writing playlist %s: %s\n", playlist_name.c_str(), strerror(errno));
         return;
      }
      
      for(size_t i = 0; i < filenames.size(); i++)
      {
         string src_file = filenames[i];

         // Don't write the file to the playlist if it doesn't actually exist.
         if(!FileReadable(pfc::stringcvt::string_wide_from_utf8(src_file.c_str()).get_ptr()))
            continue;

         // Write the line for the current item in the output playlist.
         fprintf(playlist, "%s\n", basename(src_file).c_str());
      }

      // Cleanup.
      fclose( playlist );
   }
}

// Write the specified playlist to the target directory set in the configuration
// options, modified to point to files in the same directory.
void Sync::make_playlists(t_size playlist_id_in)
{
   // Figure out the remote name to write the playlist to.
   pfc::string8 playlist_name;
   static_api_ptr_t<playlist_manager>()->playlist_get_name(playlist_id_in, playlist_name);
   playlist_name << ".m3u";

   vector<string> &filenames = playlists[playlist_name.get_ptr()];

   // Build the source path list.
   pfc::list_t<metadb_handle_ptr> playlist_items;
   static_api_ptr_t<playlist_manager>()->playlist_get_all_items(playlist_id_in, playlist_items);

   for(t_size i = 0; i < playlist_items.get_count(); i++) {
      pfc::string8 src_file;
      filesystem::g_get_display_path(playlist_items.get_item(i).get_ptr()->get_path(), src_file);
      filenames.push_back(src_file.get_ptr());
   }
}

// Check the files in the target directory and remove any that aren't playlists 
// or music files.
void Sync::remove_files()
{
   WIN32_FIND_DATA find_data;
   HANDLE find_handle = INVALID_HANDLE_VALUE;
   DWORD result_error = 0;
   pfc::string8 search_target,
      delete_file_name;
   t_size playlist_idx_iter;
   bool item_found;

   // Clean up a string with the target path.
   search_target << cfg_targetpath;
   if( !search_target.ends_with( '\\' ) ) {
      // FindFirstFile and co. don't like trailing backslashes.
      search_target.add_chars( '\\', 1 );
   }
   search_target << "*.mp3";

   // Find the first file in the directory.
   find_handle = FindFirstFile( pfc::stringcvt::string_wide_from_utf8( search_target ), &find_data );
   if( INVALID_HANDLE_VALUE == find_handle ) {
      popup_message::g_show(
         pfc::string8() << "There was a problem examining the directory for removal.",
         APP_NAME
      );
      return;
   } 
   
   do {
      // Check the file against the list of all copied files and delete it if 
      // it's not present.
      // TODO: Is there a more effective/quicker way of doing this? We're not 
      //       very good with algorithms.
      item_found = false;
      for( 
         playlist_idx_iter = 0;
         playlist_idx_iter < all_playlist_items.size();
         playlist_idx_iter++
      ) {
         if( !wcscmp(find_data.cFileName, all_playlist_items[playlist_idx_iter].c_str()) ) {
            // The item from the file system is present in the list, so move 
            // on to the next step.
            item_found = true;
            break;
         }
      }

      if( !item_found ) {
         // The filename could not be found in the list of playlist files, so
         // delete the file.
         delete_file_name.reset();
         delete_file_name <<
            pfc::stringcvt::string_utf8_from_wide( find_data.cFileName );

         pfc::string8 delete_file_path;
         androidsync_remote( delete_file_name, delete_file_path );
         DeleteFile( pfc::stringcvt::string_wide_from_utf8( delete_file_path ) );
      }     
   } while( 0 != FindNextFile( find_handle, &find_data ) );

   FindClose( find_handle );
}

void Sync::init_sync()
{
   set<string> selected_playlists;
   for(size_t i = 0; i < cfg_selectedplaylists.get_count(); ++i)
   {
      selected_playlists.insert((const char *) cfg_selectedplaylists.get_item(i));
   }

   for(size_t i = 0; i < static_api_ptr_t<playlist_manager>()->get_playlist_count(); i++) {
      pfc::string8 plist_iter;
      static_api_ptr_t<playlist_manager>()->playlist_get_name(i, plist_iter);
      if(selected_playlists.find((const char *) plist_iter) == selected_playlists.end())
         continue;

      // This playlist is on the list of selected playlists.
      make_playlists(i);
      make_item_list(i);
   }
}

void Sync::perform_sync()
{
   purge_files();

   // Write playlists first, so if the user cancels partway through his playlists are
   // available for the files that finished.
   write_playlists();
   copy_files();

   // Remove files in the target directory which aren't music or playlist 
   // files.
   // remove_files();

   // Don't log this.  It's annoying to pop open the notification box telling the user what he
   // just did.
   // if(cancelled)
   //    log += "Synchronization was aborted by the user.\n";

   // Report results.
   if(!log.empty())
      popup_message::g_show(log.c_str(), APP_NAME);
}

class ThreadedSync
{
public:
   ThreadedSync()
   {
      finished = false;
      thread = INVALID_HANDLE_VALUE;

      sync.init_sync();
   }

   const Sync &getSync() { return sync; }
   void begin()
   {
      DWORD threadId;
      thread = CreateThread(NULL, 0, sync_thread, this, 0, &threadId);
      assert(thread != INVALID_HANDLE_VALUE);
   }

   void wait()
   {
      SimpleWaitForSingleObject(thread, INFINITE);
      CloseHandle(thread);
   }

   void cancel()
   {
      sync.cancel();
   }

   bool get_finished() const { return finished; }

private:
   static DWORD WINAPI sync_thread( LPVOID pData )
   {
      ((ThreadedSync *) pData)->thread_main();
      return 0;
   }

   void thread_main()
   {
      try {
         sync.perform_sync();
      } catch(...) {
         finished = true;
         throw;
      }
      finished = true;
   }

   Sync sync;
   bool finished;

   HANDLE thread;
};

static ThreadedSync *threadedSync = NULL;
BOOL CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
   switch( msg )
   {
   case WM_INITDIALOG:
   {
       modeless_dialog_manager::g_add(hWnd);

       SendMessage(GetDlgItem(hWnd, IDC_PROGRESS1), PBM_SETRANGE, 0, MAKELPARAM(0, 1000));
       SendMessage(GetDlgItem(hWnd, IDC_PROGRESS1), PBM_SETPOS, 0, 0);

       SetTimer(hWnd, 1, 50, NULL);
       threadedSync->begin();
       ShowWindow(hWnd, SW_SHOW); 
       return TRUE;
   }

   case WM_COMMAND:
   {
      int item = LOWORD (wParam);
      int cmd = HIWORD (wParam);
      if(cmd == BN_CLICKED)
      {
         if(item == IDCANCEL)
            threadedSync->cancel();
      }
      return TRUE;
   }

   case WM_TIMER:
   {
      int progress = threadedSync->getSync().get_progress();
      // Windows 7 progress meters interpolate changes.  This is completely broken: progress
      // meters never actually reach a full state, because it's still animating the previous
      // change when the window closes.  Work around this; this has the effect of disabling
      // progress animations entirely.
      SendMessage(GetDlgItem(hWnd, IDC_PROGRESS1), PBM_SETPOS, progress, 0);
      SendMessage(GetDlgItem(hWnd, IDC_PROGRESS1), PBM_SETPOS, progress-1, 0);
      SendMessage(GetDlgItem(hWnd, IDC_PROGRESS1), PBM_SETPOS, progress, 0);

      if(threadedSync->get_finished())
      {
         // Wait for the sync thread to exit.
         threadedSync->wait();

         // Close the dialog.
         DestroyWindow(hWnd);
         return TRUE;
      }

      return TRUE;
   }

   case WM_DESTROY:
      delete threadedSync;
      threadedSync = NULL;

      modeless_dialog_manager::g_remove(hWnd);

      return TRUE;
   }
   return FALSE;
}

void androidsync_do_sync() {
   // Stop if we're already running.
   if(threadedSync != NULL)
      return;

//   AllocConsole();
//   freopen( "CONOUT$","wb", stdout );
//   freopen( "CONOUT$","wb", stderr );

   // Make sure the target path is available. Quit if it's not.
   if( GetFileAttributes(pfc::stringcvt::string_wide_from_utf8(cfg_targetpath)) == 0xFFFFFFFF )
   {
      popup_message::g_show(
         pfc::string8() << "The target path specified, \"" << cfg_targetpath <<
            "\", is not available. Please mount your Android device or fix " <<
            "the path specified in the " << APP_NAME << " options.",
         APP_NAME
      );
      return;
   }

   // Set up the helper that'll run the actual sync in a thread.
   threadedSync = new ThreadedSync;

   // Set up the dialog.  When this returns, the sync finished or was cancelled.
   CreateDialog(core_api::get_my_instance(), MAKEINTRESOURCE(IDD_SYNC), core_api::get_main_window(), WndProc);
}
