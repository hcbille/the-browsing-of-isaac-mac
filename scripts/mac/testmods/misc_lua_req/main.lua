-- misc_lua_req: a mod that requires one file, for lab_misc_lua.lua (Isaac.GetLoadedModules, Debug.ListLoadedFiles, Debug.ForceUnload).
local mod = RegisterMod("misc lua req", 1)
MISC_LUA_REQ_COUNT = 0
require("misc_lua_req_helper")
