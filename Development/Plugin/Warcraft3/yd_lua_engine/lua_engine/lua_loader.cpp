#include "callback.h"
#include "lua_helper.h"
#include "lua_loader.h"
#include "storm.h"
#include "open_lua_engine.h"
#include "libs_runtime.h"
#include <lua.hpp>
#include <base/warcraft3/event.h>
#include <base/warcraft3/jass.h>
#include <base/warcraft3/jass/func_value.h>
#include <base/warcraft3/jass/hook.h>
#include <base/util/singleton.h>
#include <base/util/format.h>
#include <base/hook/fp_call.h>
#include <base/util/string_view.h>	
#include <base/util/string_algorithm.h>

namespace base { namespace warcraft3 { namespace lua_engine { namespace lua_loader {

	class jass_state
	{
	public:
		jass_state()
			: state_(nullptr)
		{
			register_game_reset_event([this](uintptr_t)
			{
				if (state_)
				{
					lua_close(state_);
					state_ = nullptr;
				}
			});
		}

		lua_State* get()
		{
			if (!state_) {
				state_ = initialize();
				luaL_dostring(state_, "(require 'jass.debugger').listen('127.0.0.1', 4278)");
			}
			return state_;
		}

	private:
		lua_State* initialize()
		{
			lua_State* L = luaL_newstate2();
			luaL_openlibs(L);
			clear_searchers_table(L);
			open_lua_engine(L);
			runtime::initialize();
			return L;
		}

	private:
		lua_State* state_; 
	};
	typedef singleton_nonthreadsafe<jass_state> jass_state_s;

	uintptr_t RealCheat = 0;
	void __cdecl FakeCheat(jass::jstring_t cheat_str)
	{
		const char* cheat = jass::from_string(cheat_str);

		if (!cheat)
		{
			c_call<uint32_t>(RealCheat, cheat_str);
			return ;
		}

		std::string_view cheat_s = cheat;
		
		if (cheat_s.substr(0, 9) == "exec-lua:")
		{
			cheat_s = cheat_s.substr(9);
			algorithm::trim(cheat_s);
			if (cheat_s.size() >= 2 && cheat_s[0] == '"' && cheat_s[cheat_s.size() - 1] == '"')
			{
				cheat_s = cheat_s.substr(1, cheat_s.size() - 2);
			}
			lua_State* L = jass_state_s::instance().get();
			lua_getglobal(L, "require");
			lua_pushlstring(L, cheat_s.data(), cheat_s.size());
			safe_call(L, 1, 1, true);
		}

		c_call<uint32_t>(RealCheat, cheat_str);
	}


	jass::jstring_t __cdecl EXExecuteScript(jass::jstring_t script)
	{
		lua_State* L = jass_state_s::instance().get();

		std::string str_script = format("return (%s)", jass::from_trigstring(jass::from_string(script)));
		if (luaL_loadbuffer(L, str_script.c_str(), str_script.size(), str_script.c_str()) != LUA_OK)
		{
			printf("%s\n", lua_tostring(L, -1));
			lua_pop(L, 1);
			return 0;
		}

		if (LUA_OK != safe_call(L, 0, 1, true))
		{
			return 0;
		}

		jass::jstring_t result = 0;
		if (lua_isstring(L, -1))
		{
			result = jass::create_string(lua_tostring(L, -1));
		}
		lua_pop(L, 1);
		return result;
	}

	void initialize()
	{
		jass::async_hook("Cheat", (uintptr_t*)&RealCheat, (uintptr_t)FakeCheat);
		jass::japi_add((uintptr_t)EXExecuteScript, "EXExecuteScript", "(S)S");
	}
}}}}
