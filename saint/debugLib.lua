local _debug = debug
local debug_funcs = {}
env.debug = {}

debug_funcs.getinfo = function(f)
	type_check(1, f, { "number", "function" })

	if not pcall(getfenv, f) then
		error("invalid stack detected", 0)
	end

	if f == 0 then
		f = 1
	end
	if type(f) == "number" then
		f += 1
	end

	local s, n, a, v, l, fn = debug.info(f, "snalf")

	return {
		source = s,
		short_src = s,
		func = fn,
		what = (s == "[C]" and "C") or "Lua",
		currentline = l,
		name = n,
		nups = -1,
		numparams = a,
		is_vararg = (v and 1) or 0,
	}
end

debug_funcs.getconstant = function(f, index)
	type_check(1, f, { "function", "number" })
	type_check(2, index, { "number" })

	if type(f) == "number" then
		f += 1
		if not pcall(getfenv, f + 1) then
			error("invalid stack level", 0)
		end
	end

	local decomp = decompile(debug.info(f, "f"))
	local constants = decomp[2] -- constant table

	return constants[index]
end

debug_funcs.getconstants = function(f)
	type_check(1, f, { "function", "number" })

	if type(f) == "number" then
		f += 1
		if not pcall(getfenv, f + 1) then
			error("invalid stack level", 0)
		end
	end

	local decomp = decompile(debug.info(f, "f"))
	return decomp[2] -- the entire constant array
end

debug_funcs.getproto = function(f, index, active)
	type_check(1, f, { "function", "number" })
	type_check(2, index, { "number" })
	type_check(3, active, { "boolean" }, true) -- active default = true

	if type(f) == "number" then
		f += 1
		if not pcall(getfenv, f + 1) then
			error("invalid stack level", 0)
		end
	end

	local decomp = decompile(debug.info(f, "f"))
	local proto = decomp[3][index]

	if active then
		return { proto }
	else
		return proto
	end
end

debug_funcs.getprotos = function(f)
	type_check(1, f, { "function", "number" })

	if type(f) == "number" then
		f += 1
		if not pcall(getfenv, f + 1) then
			error("invalid stack level", 0)
		end
	end

	local decomp = decompile(debug.info(f, "f"))
	return decomp[3] -- array of protos
end

setmetatable(env.debug, {
	__index = function(_, key)
		if debug_funcs[key] then
			return debug_funcs[key] -- custom debug funcs
		end
		return _debug[key] -- still able to use built in funcs
	end,
	__metatable = getmetatable(_debug),
})
