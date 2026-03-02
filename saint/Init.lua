

local cg = game:GetService("CoreGui")
local hs = game:GetService("HttpService")
local is = game:GetService("InsertService")
local ps = game:GetService("Players")

local RKO = Instance.new("Folder", cg)
RKO.Name = "RKO"
local Pointer = Instance.new("Folder", RKO)
Pointer.Name = "Pointer"
local Bridge = Instance.new("Folder", RKO)
Bridge.Name = "Bridge"

local plr = ps.LocalPlayer

local rtypeof = typeof

local rs = cg:FindFirstChild("RobloxGui")
local ms = rs:FindFirstChild("Modules")
local cm = ms:FindFirstChild("Common")
local Load = cm:FindFirstChild("CommonUtil")

local BridgeUrl = "http://localhost:9611"
local ProcessID = "%-PROCESS-ID-%"
local Vernushwd = "RKO-HWID-" .. plr.UserId

local resc = 3
local function bsend(dta, typ, set)
    local timeout = 5
    local result = nil
    local clock = tick()
    
    -- Ensure clock is a number
    if type(clock) ~= "number" or clock == nil then
        clock = 0
    end

    dta = dta or ""
    typ = typ or "none"
    set = set or {}

    local requestCompleted = false
    local responseBody = ""
    local responseSuccess = false

    -- Make the HTTP request
    local success, request = pcall(function()
        return hs:RequestInternal({
            Url = BridgeUrl .. "/handle",
            Body = typ .. "\n" .. ProcessID .. "\n" .. hs:JSONEncode(set) .. "\n" .. dta,
            Method = "POST",
            Headers = {
                ['Content-Type'] = "text/plain",
            }
        })
    end)
    
    if not success or request == nil then
        return ""
    end
    
    -- Handle the response
    local connection
    connection = request:Start(function(success, response)
        responseSuccess = success
        if success and response then
            responseBody = response.Body or ""
        else
            responseBody = ""
        end
        requestCompleted = true
        if connection then
            connection:Disconnect()
        end
    end)

    -- Wait for response with timeout
    local startTime = os.clock()
    while not requestCompleted do 
        task.wait(0.1)
        local elapsed = os.clock() - startTime
        if elapsed > timeout then
            if connection then
                connection:Disconnect()
            end
            break
        end
    end

    if not responseSuccess then
        -- Ensure resc is a number before comparing
        if resc == nil or type(resc) ~= "number" then
            resc = 3
        end
        if resc <= 0 then
            local StarterGui = game:GetService("StarterGui")

StarterGui:SetCore("SendNotification", {
    Title = "[RKO]",
    Text = 'Your files will no longer save',
    Duration = 5,
})
            return ""
        else
            resc = resc - 1
        end
    else
        resc = 3
    end

    return responseBody
end
local env = getfenv(function() end)



env.identifyexecutor = function()
	return "RKO", "1.0.0"
end
env.getexecutorname = env.identifyexecutor

env.compile = function(code : string, encoded : bool)
	local code = typeof(code) == "string" and code or ""
	local encoded = typeof(encoded) == "boolean" and encoded or false
	local res = bsend(code, "compile", {
		["enc"] = tostring(encoded)
	})
	return res or ""
end

env.setscriptbytecode = function(script : Instance, bytecode : string)
	local obj = Instance.new("ObjectValue", Pointer)
	obj.Name = hs:GenerateGUID(false)
	obj.Value = script

	bsend(bytecode, "setscriptbytecode", {
		["cn"] = obj.Name
	})

	obj:Destroy()
end

local clonerefs = {}
env.cloneref = function(obj)
	local proxy = newproxy(true)
	local meta = getmetatable(proxy)
	meta.__index = function(t, n)
		local v = obj[n]
		if typeof(v) == "function" then
			return function(self, ...)
				if self == t then
					self = obj
				end
				return v(self, ...)
			end
		else
			return v
		end
	end
	meta.__newindex = function(t, n, v)
		obj[n] = v
	end
	meta.__tostring = function(t)
		return tostring(obj)
	end
	meta.__metatable = getmetatable(obj)
	clonerefs[proxy] = obj
	
	return proxy
end

env.compareinstances = function(proxy1, proxy2)
	assert(type(proxy1) == "userdata", "Invalid argument #1 to 'compareinstances' (Instance expected, got " .. typeof(proxy1) .. ")")
	assert(type(proxy2) == "userdata", "Invalid argument #2 to 'compareinstances' (Instance expected, got " .. typeof(proxy2) .. ")")
	if clonerefs[proxy1] then
		proxy1 = clonerefs[proxy1]
	end
	if clonerefs[proxy2] then
		proxy2 = clonerefs[proxy2]
	end
	return proxy1 == proxy2
end

env.loadstring = function(code, chunkname)
    assert(type(code) == "string", "invalid argument #1 to 'loadstring' (string expected, got " .. type(code) .. ") ", 2)
    chunkname = chunkname or "loadstring"
    assert(type(chunkname) == "string", "invalid argument #2 to 'loadstring' (string expected, got " .. type(chunkname) .. ") ", 2)
    chunkname = chunkname:gsub("[^%a_]", "")
    if chunkname == "" then
        chunkname = "loadstring"
    end
    if (code == "" or code == " ") then
        return nil, "Empty script source"
    end

    local function looks_like_bytecode(s)
        if type(s) ~= "string" then
            return false
        end
        local header4 = string.sub(s, 1, 4)
        if header4 == "RSB1" or header4 == "Luau" then
            return true
        end
        if string.find(s, "%z", 1, true) then
            return true
        end
        local bad = 0
        local limit = math.min(#s, 256)
        for i = 1, limit do
            local b = string.byte(s, i)
            if b < 9 or (b > 13 and b < 32) then
                bad += 1
                if bad >= 3 then
                    return true
                end
            end
        end
        return false
    end

    if looks_like_bytecode(code) then
        return nil, "Luau bytecode is not supported"
    end

    local okc, bytecode = pcall(env.compile, "return{[ [["..chunkname.."]] ]=function(...)local roe=function()return'\67\104\105\109\101\114\97\76\108\101'end;"..code.."\nend}", true)
    if not okc or type(bytecode) ~= "string" or #bytecode <= 1 then
        return nil, "Compile Failed!"
    end

    local oksb = pcall(env.setscriptbytecode, Load, bytecode)
    if not oksb then
        return nil, "Failed To Load!"
    end

    local suc, res = pcall(function()
        return debug.loadmodule(Load)
    end)

    if suc then
        local suc2, res2 = pcall(function()
            return res()
        end)
        if suc2 and typeof(res2) == "table" and typeof(res2[chunkname]) == "function" then
            return setfenv(res2[chunkname], env)
        else
            return nil, "Failed To Load!"
        end
    else
        return nil, (res or "Failed To Load!")
    end
end



local lookupValueToCharacter = buffer.create(64)
local lookupCharacterToValue = buffer.create(256)

local alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"
local padding = string.byte("=")

for index = 1, 64 do
	local value = index - 1
	local character = string.byte(alphabet, index)

	buffer.writeu8(lookupValueToCharacter, value, character)
	buffer.writeu8(lookupCharacterToValue, character, value)
end

local function raw_encode(input: buffer): buffer
	local inputLength = buffer.len(input)
	local inputChunks = math.ceil(inputLength / 3)

	local outputLength = inputChunks * 4
	local output = buffer.create(outputLength)

	for chunkIndex = 1, inputChunks - 1 do
		local inputIndex = (chunkIndex - 1) * 3
		local outputIndex = (chunkIndex - 1) * 4

		local chunk = bit32.byteswap(buffer.readu32(input, inputIndex))

		local value1 = bit32.rshift(chunk, 26)
		local value2 = bit32.band(bit32.rshift(chunk, 20), 0b111111)
		local value3 = bit32.band(bit32.rshift(chunk, 14), 0b111111)
		local value4 = bit32.band(bit32.rshift(chunk, 8), 0b111111)

		buffer.writeu8(output, outputIndex, buffer.readu8(lookupValueToCharacter, value1))
		buffer.writeu8(output, outputIndex + 1, buffer.readu8(lookupValueToCharacter, value2))
		buffer.writeu8(output, outputIndex + 2, buffer.readu8(lookupValueToCharacter, value3))
		buffer.writeu8(output, outputIndex + 3, buffer.readu8(lookupValueToCharacter, value4))
	end

	local inputRemainder = inputLength % 3

	if inputRemainder == 1 then
		local chunk = buffer.readu8(input, inputLength - 1)

		local value1 = bit32.rshift(chunk, 2)
		local value2 = bit32.band(bit32.lshift(chunk, 4), 0b111111)

		buffer.writeu8(output, outputLength - 4, buffer.readu8(lookupValueToCharacter, value1))
		buffer.writeu8(output, outputLength - 3, buffer.readu8(lookupValueToCharacter, value2))
		buffer.writeu8(output, outputLength - 2, padding)
		buffer.writeu8(output, outputLength - 1, padding)
	elseif inputRemainder == 2 then
		local chunk = bit32.bor(
			bit32.lshift(buffer.readu8(input, inputLength - 2), 8),
			buffer.readu8(input, inputLength - 1)
		)

		local value1 = bit32.rshift(chunk, 10)
		local value2 = bit32.band(bit32.rshift(chunk, 4), 0b111111)
		local value3 = bit32.band(bit32.lshift(chunk, 2), 0b111111)

		buffer.writeu8(output, outputLength - 4, buffer.readu8(lookupValueToCharacter, value1))
		buffer.writeu8(output, outputLength - 3, buffer.readu8(lookupValueToCharacter, value2))
		buffer.writeu8(output, outputLength - 2, buffer.readu8(lookupValueToCharacter, value3))
		buffer.writeu8(output, outputLength - 1, padding)
	elseif inputRemainder == 0 and inputLength ~= 0 then
		local chunk = bit32.bor(
			bit32.lshift(buffer.readu8(input, inputLength - 3), 16),
			bit32.lshift(buffer.readu8(input, inputLength - 2), 8),
			buffer.readu8(input, inputLength - 1)
		)

		local value1 = bit32.rshift(chunk, 18)
		local value2 = bit32.band(bit32.rshift(chunk, 12), 0b111111)
		local value3 = bit32.band(bit32.rshift(chunk, 6), 0b111111)
		local value4 = bit32.band(chunk, 0b111111)

		buffer.writeu8(output, outputLength - 4, buffer.readu8(lookupValueToCharacter, value1))
		buffer.writeu8(output, outputLength - 3, buffer.readu8(lookupValueToCharacter, value2))
		buffer.writeu8(output, outputLength - 2, buffer.readu8(lookupValueToCharacter, value3))
		buffer.writeu8(output, outputLength - 1, buffer.readu8(lookupValueToCharacter, value4))
	end

	return output
end

local function raw_decode(input: buffer): buffer
	local inputLength = buffer.len(input)
	local inputChunks = math.ceil(inputLength / 4)

	local inputPadding = 0
	if inputLength ~= 0 then
		if buffer.readu8(input, inputLength - 1) == padding then inputPadding += 1 end
		if buffer.readu8(input, inputLength - 2) == padding then inputPadding += 1 end
	end

	local outputLength = inputChunks * 3 - inputPadding
	local output = buffer.create(outputLength)

	for chunkIndex = 1, inputChunks - 1 do
		local inputIndex = (chunkIndex - 1) * 4
		local outputIndex = (chunkIndex - 1) * 3

		local value1 = buffer.readu8(lookupCharacterToValue, buffer.readu8(input, inputIndex))
		local value2 = buffer.readu8(lookupCharacterToValue, buffer.readu8(input, inputIndex + 1))
		local value3 = buffer.readu8(lookupCharacterToValue, buffer.readu8(input, inputIndex + 2))
		local value4 = buffer.readu8(lookupCharacterToValue, buffer.readu8(input, inputIndex + 3))

		local chunk = bit32.bor(
			bit32.lshift(value1, 18),
			bit32.lshift(value2, 12),
			bit32.lshift(value3, 6),
			value4
		)

		local character1 = bit32.rshift(chunk, 16)
		local character2 = bit32.band(bit32.rshift(chunk, 8), 0b11111111)
		local character3 = bit32.band(chunk, 0b11111111)

		buffer.writeu8(output, outputIndex, character1)
		buffer.writeu8(output, outputIndex + 1, character2)
		buffer.writeu8(output, outputIndex + 2, character3)
	end

	if inputLength ~= 0 then
		local lastInputIndex = (inputChunks - 1) * 4
		local lastOutputIndex = (inputChunks - 1) * 3

		local lastValue1 = buffer.readu8(lookupCharacterToValue, buffer.readu8(input, lastInputIndex))
		local lastValue2 = buffer.readu8(lookupCharacterToValue, buffer.readu8(input, lastInputIndex + 1))
		local lastValue3 = buffer.readu8(lookupCharacterToValue, buffer.readu8(input, lastInputIndex + 2))
		local lastValue4 = buffer.readu8(lookupCharacterToValue, buffer.readu8(input, lastInputIndex + 3))

		local lastChunk = bit32.bor(
			bit32.lshift(lastValue1, 18),
			bit32.lshift(lastValue2, 12),
			bit32.lshift(lastValue3, 6),
			lastValue4
		)

		if inputPadding <= 2 then
			local lastCharacter1 = bit32.rshift(lastChunk, 16)
			buffer.writeu8(output, lastOutputIndex, lastCharacter1)

			if inputPadding <= 1 then
				local lastCharacter2 = bit32.band(bit32.rshift(lastChunk, 8), 0b11111111)
				buffer.writeu8(output, lastOutputIndex + 1, lastCharacter2)

				if inputPadding == 0 then
					local lastCharacter3 = bit32.band(lastChunk, 0b11111111)
					buffer.writeu8(output, lastOutputIndex + 2, lastCharacter3)
				end
			end
		end
	end

	return output
end

env.base64encode = function(input)
	return buffer.tostring(raw_encode(buffer.fromstring(input)))
end
env.base64_encode = env.base64encode

env.base64decode = function(encoded)
	return buffer.tostring(raw_decode(buffer.fromstring(encoded)))
end
env.base64_decode = env.base64decode

local base64 = {}

base64.encode = env.base64encode
base64.decode = env.base64decode

env.base64 = base64

env.islclosure = function(func)
	assert(type(func) == "function", "invalid argument #1 to 'islclosure' (function expected, got " .. type(func) .. ") ", 2)
	return debug.info(func, "s") ~= "[C]"
end
env.isluaclosure = env.islclosure

env.iscclosure = function(func)
	assert(type(func) == "function", "invalid argument #1 to 'iscclosure' (function expected, got " .. type(func) .. ") ", 2)
	return debug.info(func, "s") == "[C]"
end

env.newlclosure = function(func)
	assert(type(func) == "function", "invalid argument #1 to 'newlclosure' (function expected, got " .. type(func) .. ") ", 2)
	local cloned = function(...)
		return func(...)
	end
	return cloned
end

env.newcclosure = function(func)
	assert(type(func) == "function", "invalid argument #1 to 'newcclosure' (function expected, got " .. type(func) .. ") ", 2)
	local cloned = coroutine.wrap(function(...)
		while true do
			coroutine.yield(func(...))
		end
	end)
	return cloned
end



env.clonefunction = function(func)
	assert(type(func) == "function", "invalid argument #1 to 'clonefunction' (function expected, got " .. type(func) .. ") ", 2)
	if env.iscclosure(func) then
		return env.newcclosure(func)
	else
		return env.newlclosure(func)
	end
end
local supportedMethods = {"GET", "POST", "PUT", "DELETE", "PATCH"}
env.request = function(options)
    assert(type(options) == "table", "invalid argument #1 to 'request' (table expected, got " .. type(options) .. ") ", 2)
    assert(type(options.Url) == "string", "invalid option 'Url' for argument #1 to 'request' (string expected, got " .. type(options.Url) .. ") ", 2)
    options.Method = options.Method or "GET"
    options.Method = options.Method:upper()
    assert(table.find(supportedMethods, options.Method), "invalid option 'Method' for argument #1 to 'request' (a valid http method expected, got '" .. options.Method .. "') ", 2)
    assert(not (options.Method == "GET" and options.Body), "invalid option 'Body' for argument #1 to 'request' (current method is GET but option 'Body' was used)", 2)
    
    if options.Body then
        assert(type(options.Body) == "string", "invalid option 'Body' for argument #1 to 'request' (string expected, got " .. type(options.Body) .. ") ", 2)
    end
    
    if options.Headers then 
        assert(type(options.Headers) == "table", "invalid option 'Headers' for argument #1 to 'request' (table expected, got " .. type(options.Headers) .. ") ", 2) 
    end
    
    options.Body = options.Body or "{}"
    options.Headers = options.Headers or {}
    
    if (options.Headers["User-Agent"]) then 
        assert(type(options.Headers["User-Agent"]) == "string", "invalid option 'User-Agent' for argument #1 to 'request.Header' (string expected, got " .. type(options.Headers["User-Agent"]) .. ") ", 2) 
    end
    
    options.Headers["User-Agent"] = options.Headers["User-Agent"] or "RKO"
    options.Headers["RKO-Fingerprint"] = Vernushwd
    options.Headers["Cache-Control"] = "no-cache"
    options.Headers["Roblox-Place-Id"] = tostring(game.PlaceId)
    options.Headers["Roblox-Game-Id"] = tostring(game.JobId)
    options.Headers["Roblox-Session-Id"] = hs:JSONEncode({
        ["GameId"] = tostring(game.GameId),
        ["PlaceId"] = tostring(game.PlaceId)
    })
    
    -- Send the request through bsend
    local res = bsend("", "request", {
        ['l'] = options.Url,
        ['m'] = options.Method,
        ['h'] = options.Headers,
        ['b'] = options.Body or "{}"
    })
    
    if res and res ~= "" then
        -- Try to parse the response as JSON
        local success, result = pcall(function() 
            return hs:JSONDecode(res) 
        end)
        
        if success and type(result) == "table" then
            -- Handle the response format from your server
            local statusCode = tonumber(result['c']) or tonumber(result['StatusCode']) or 0
            local statusMessage = result['r'] or result['StatusMessage'] or "Unknown"
            local body = result['b'] or result['Body'] or ""
            local headers = result['h'] or result['Headers'] or {}
            local httpVersion = result['v'] or result['Version'] or "1.1"
            
            return {
                Success = statusCode >= 200 and statusCode < 300,
                StatusMessage = statusMessage,
                StatusCode = statusCode,
                Body = body,
                Headers = headers,
                Version = httpVersion
            }
        else
            -- If it's not JSON, assume it's the raw response body
            return {
                Success = true,
                StatusMessage = "OK",
                StatusCode = 200,
                Body = res,
                Headers = {},
                Version = "1.1"
            }
        end
    else
        -- No response or empty response
        return {
            Success = false,
            StatusMessage = "No response from server",
            StatusCode = 0,
            Body = "",
            Headers = {},
            Version = "1.1"
        }
    end
end
local user_agent = "Roblox/WinInet"
function env.HttpGet(url, returnRaw)
	assert(type(url) == "string", "invalid argument #1 to 'HttpGet' (string expected, got " .. type(url) .. ") ", 2)
	local returnRaw = returnRaw or true

	local result = env.request({
		Url = url,
		Method = "GET",
		Headers = {
			["User-Agent"] = user_agent
		}
	})

	if returnRaw then
		return result.Body
	end

	return hs:JSONDecode(result.Body)
end
function env.HttpPost(url, body, contentType)
	assert(type(url) == "string", "invalid argument #1 to 'HttpPost' (string expected, got " .. type(url) .. ") ", 2)
	contentType = contentType or "application/json"
	return env.request({
		Url = url,
		Method = "POST",
		body = body,
		Headers = {
			["Content-Type"] = contentType
		}
	})
end
function env.GetObjects(asset)
	return {
		is:LoadLocalAsset(asset)
	}
end

local function GenerateError(object)
	local _, err = xpcall(function()
		object:__namecall()
	end, function()
		return debug.info(2, "f")
	end)
	return err
end

local FirstTest = GenerateError(OverlapParams.new())
local SecondTest = GenerateError(Color3.new())

local cachedmethods = {}
env.getnamecallmethod = function()
	local _, err = pcall(FirstTest)
	local method = if type(err) == "string" then err:match("^(.+) is not a valid member of %w+$") else nil
	if not method then
		_, err = pcall(SecondTest)
		method = if type(err) == "string" then err:match("^(.+) is not a valid member of %w+$") else nil
	end
	local fixerdata = newproxy(true)
	local fixermeta = getmetatable(fixerdata)
	fixermeta.__namecall = function()
		local _, err = pcall(FirstTest)
		local method = if type(err) == "string" then err:match("^(.+) is not a valid member of %w+$") else nil
		if not method then
			_, err = pcall(SecondTest)
			method = if type(err) == "string" then err:match("^(.+) is not a valid member of %w+$") else nil
		end
	end
	fixerdata:__namecall()
	if not method or method == "__namecall" then
		if cachedmethods[coroutine.running()] then
			return cachedmethods[coroutine.running()]
		end
		return nil
	end
	cachedmethods[coroutine.running()] = method
	return method
end

local proxyobject
local proxied = {}
local objects = {}
local scriptableProperties = setmetatable({}, { __mode = "k" })
function ToProxy(...)
	local packed = table.pack(...)
	local function LookTable(t)
		for i, obj in ipairs(t) do
			if rtypeof(obj) == "Instance" then
				if objects[obj] then
					t[i] = objects[obj].proxy
				else
					t[i] = proxyobject(obj)
				end
			elseif typeof(obj) == "table" then
				LookTable(obj)
			else
				t[i] = obj
			end
		end
	end
	LookTable(packed)
	return table.unpack(packed, 1, packed.n)
end

function ToObject(...)
	local packed = table.pack(...)
	local function LookTable(t)
		for i, obj in ipairs(t) do
			if rtypeof(obj) == "userdata" then
				if proxied[obj] then
					t[i] = proxied[obj].object
				else
					t[i] = obj
				end
			elseif typeof(obj) == "table" then
				LookTable(obj)
			else
				t[i] = obj
			end
		end
	end
	LookTable(packed)
	return table.unpack(packed, 1, packed.n)
end
local function index(t, n)
    local data = proxied[t]
    
    -- If this isn't a proxied object, return the real value
    if not data then
        return t[n]
    end
    
    local namecalls = data.namecalls
    local obj = data.object
    
    if namecalls[n] then
        return function(self, ...)
            return ToProxy(namecalls[n](...))
        end
    end
    
    -- Check if this property is marked as scriptable
    if scriptableProperties[obj] and scriptableProperties[obj][n] then
        -- Property is marked as scriptable, try to access it
        
        -- First try normal access
        local success, value = pcall(function()
            return obj[n]
        end)
        
        if success and value ~= nil then
            return ToProxy(value)
        end
        
        -- If normal access failed, try hidden property
        local hiddenSuccess, hiddenValue = pcall(function()
            return env.gethiddenproperty(obj, n)
        end)
        
        if hiddenSuccess and hiddenValue ~= nil then
            return ToProxy(hiddenValue)
        end
        
        -- If both fail and property is marked as scriptable, return nil
        -- This matches typical executor behavior
        return nil
    end
    
    -- Normal property access - let it error if the property doesn't exist
    -- This is important for preserving normal Roblox behavior
    local v = obj[n]
    if typeof(v) == "function" then
        return function(self, ...)
            return ToProxy(v(ToObject(self, ...)))
        end
    else
        return ToProxy(v)
    end
end

local function namecall(t, ...)
	local data = proxied[t]
	local namecalls = data.namecalls
	local obj = data.object
	local method = env.getnamecallmethod()
	if namecalls[method] then
		return ToProxy(namecalls[method](...))
	end
	return ToProxy(obj[method](ToObject(t, ...)))
end

local logs = {}
local function sizlog(obj, log)
	if not logs[obj] then
		logs[obj] = {}
	end
	if not logs[obj][log] then
		logs[obj][log] = {}
	end
	return #logs[obj][log]
end

local function newlog(obj, log, val)
	logs[obj] = logs[obj] or {}
	logs[obj][log] = logs[obj][log] or {}

	table.insert(logs[obj][log], val)
end

local function getlastlog(obj, log)
	local list = logs[obj] and logs[obj][log]
	if not list or #list == 0 then
		return nil
	end
	return table.unpack(list[#list])
end

local function getlog(obj, log, ind)
	if not logs[obj] then
		logs[obj] = {}
	end
	if not logs[obj][log] then
		logs[obj][log] = {}
	end
	return table.unpack(logs[obj][log][ind])
end

local function newindex(t, n, v)
    local data = proxied[t]
    
    -- If this isn't a proxied object, don't do anything special
    if not data then
        -- For non-proxied objects, just set the property normally
        t[n] = v
        return
    end
    
    local obj = data.object
    local val = table.pack(ToObject(v))
    
    -- Check if this property is marked as scriptable
    if scriptableProperties[obj] and scriptableProperties[obj][n] then
        -- Property is marked as scriptable, try to set it
        
        -- First try normal set
        local success, err = pcall(function()
            obj[n] = table.unpack(val)
        end)
        
        if not success then
            -- If normal set failed, try hidden property
            local hiddenSuccess = pcall(function()
                return env.sethiddenproperty(obj, n, table.unpack(val))
            end)
            
            -- If hidden set also failed, we still don't error
            -- This matches typical executor behavior
        end
    else
        -- Normal property set
        obj[n] = table.unpack(val)
    end
end
local function ptostring(t)
    local data = proxied[t]
    if data and data.object then
        return tostring(data.object)
    end
    return tostring(t)
end

function proxyobject(obj, namecalls)
	if objects[obj] then
		return objects[obj].proxy
	end
	namecalls = namecalls or {}
	local proxy = newproxy(true)
	local meta = getmetatable(proxy)
	meta.__index = function(...)return index(...)end
	meta.__namecall = function(...)return namecall(...)end
	meta.__newindex = function(...)return newindex(...)end
	meta.__tostring = function(...)return ptostring(...)end
	meta.__metatable = getmetatable(obj)

	local data = {}
	data.object = obj
	data.proxy = proxy
	data.meta = meta
	data.namecalls = namecalls

	proxied[proxy] = data
	objects[obj] = data
	return proxy
end

local pgame = proxyobject(game, {
	HttpGet = env.HttpGet,
	HttpGetAsync = env.HttpGet,
	HttpPost = env.HttpPost,
	HttpPostAsync = env.HttpPost,
	GetObjects = env.GetObjects
})
env.game = pgame
env.Game = pgame

local pworkspace = proxyobject(workspace)
env.workspace = pworkspace
env.Workspace = pworkspace

local pscript = proxyobject(script)
env.script = pscript

local hui = proxyobject(Instance.new("ScreenGui", cg))
hui.Name = "hidden_ui_container"

for i, v in ipairs(game:GetDescendants()) do
	proxyobject(v)
end
game.DescendantAdded:Connect(proxyobject)

local rInstance = Instance
local fInstance = {}
fInstance.new = function(name, par)
	return proxyobject(rInstance.new(name, ToObject(par)))
end
fInstance.fromExisting = function(obj)
	return proxyobject(rInstance.fromExisting(obj))
end
env.Instance = fInstance

env.getinstances = function()
	local Instances = {}
	for i, v in pairs(objects) do
		table.insert(Instances, v.proxy)
	end
	return Instances
end

env.getnilinstances = function()
	local NilInstances = {}
	for i, v in pairs(objects) do
		if v.proxy.Parent == nil then
			table.insert(NilInstances, v.proxy)
		end
	end
	return NilInstances
end

env.getloadedmodules = function()
	local LoadedModules = {}
	for i, v in pairs(objects) do
		if v.proxy:IsA("ModuleScript") then
			table.insert(LoadedModules, v.proxy)
		end
	end
	return LoadedModules
end

env.getrunningscripts = function()
	local RunningScripts = {}
	for i, v in pairs(objects) do
		if v.proxy:IsA("ModuleScript") then
			table.insert(RunningScripts, v.proxy)
		end
	end
	return RunningScripts
end

env.getscripts = function()
	local Scripts = {}
	for i, v in pairs(objects) do
		if v.proxy:IsA("LocalScript") or v.proxy:IsA("ModuleScript") or v.proxy:IsA("Script") then
			table.insert(Scripts, v.proxy)
		end
	end
	return Scripts
end

env.typeof = function(obj)
	local typ = rtypeof(obj)
	if typ == "userdata" then
		if proxied[obj] then
			return "Instance"
		elseif clonerefs[obj] then
			
			local original = clonerefs[obj]
			return env.typeof(original)
		else
			return typ
		end
	else
		return typ
	end
end

env.gethui = function()
	return hui
end

local crypt = {}

crypt.base64encode = env.base64encode
crypt.base64_encode = env.base64encode
crypt.base64decode = env.base64decode
crypt.base64_decode = env.base64decode
crypt.base64 = base64

crypt.generatekey = function(len)
	local key = ''
	local x = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/'
	for i = 1, len or 32 do local n = math.random(1, #x) key = key .. x:sub(n, n) end
	return base64.encode(key)
end

crypt.encrypt = function(a, b)
	local result = {}
	a = tostring(a) b = tostring(b)
	for i = 1, #a do
		local byte = string.byte(a, i)
		local keyByte = string.byte(b, (i - 1) % #b + 1)
		table.insert(result, string.char(bit32.bxor(byte, keyByte)))
	end
	return table.concat(result), b
end

crypt.generatebytes = function(len)
	return crypt.generatekey(len)
end

crypt.random = function(len)
	return crypt.generatekey(len)
end

crypt.decrypt = crypt.encrypt

local HashRes = env.request({
	Url = "https://raw.githubusercontent.com/ChimeraLle-Real/Fynex/refs/heads/main/hash",
	Method = "GET"
})
local HashLib = {}

if HashRes and HashRes.Body then
	local func, err = env.loadstring(HashRes.Body)
	if func then
		HashLib = func()
	else
		warn("HasbLib Failed To Load Error: " .. tostring(err))
	end
end

local DrawingRes = bsend("", "drawinglib", {})
if DrawingRes and DrawingRes ~= "" then
	local func, err = env.loadstring(DrawingRes)
	if func then
		local ok, drawing = pcall(func)
		if ok and type(drawing) == "table" then
			if drawing.Drawing then
				env.Drawing = drawing.Drawing
			elseif drawing.new or drawing.Fonts then
				env.Drawing = drawing
			end
			if type(drawing.functions) == "table" then
				for i, v in pairs(drawing.functions) do
					env[i] = v
				end
			end
		else
			warn("DrawingLib Failed To Load Error: " .. tostring(drawing))
		end
	else
		warn("DrawingLib Failed To Load Error: " .. tostring(err))
	end
end

do
	local _debug = debug
	local debug_funcs = {}

	local function type_check(argIndex, value, expectedTypes, allowNil)
		if allowNil and value == nil then
			return
		end
		local t = type(value)
		for _, allowed in ipairs(expectedTypes) do
			if t == allowed then
				return
			end
		end
		error("invalid argument #" .. tostring(argIndex) .. " (" .. table.concat(expectedTypes, " or ") .. " expected, got " .. t .. ")", 3)
	end

	env.type_check = env.type_check or type_check
	env.typecheck = env.typecheck or env.type_check

	env.debug = {}

	debug_funcs.getinfo = function(f)
		env.type_check(1, f, { "number", "function" })

		if not pcall(getfenv, f) then
			error("invalid stack detected", 0)
		end

		if f == 0 then
			f = 1
		end
		if type(f) == "number" then
			f += 1
		end

		local s, n, a, v, l, fn = _debug.info(f, "snalf")

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

	local function try_decompile_for_debug(f)
		local dec = env.decompile
		if type(dec) ~= "function" then
			return nil
		end
		local ok, res = pcall(function()
			return dec(_debug.info(f, "f"))
		end)
		if not ok then
			return nil
		end
		if type(res) ~= "table" then
			return nil
		end
		return res
	end

	debug_funcs.getconstant = function(f, index)
		env.type_check(1, f, { "function", "number" })
		env.type_check(2, index, { "number" })

		if type(f) == "number" then
			f += 1
			if not pcall(getfenv, f + 1) then
				error("invalid stack level", 0)
			end
		end

		local decomp = try_decompile_for_debug(f)
		if not decomp then
			return nil
		end
		local constants = decomp[2]
		if type(constants) ~= "table" then
			return nil
		end

		return constants[index]
	end

	debug_funcs.getconstants = function(f)
		env.type_check(1, f, { "function", "number" })

		if type(f) == "number" then
			f += 1
			if not pcall(getfenv, f + 1) then
				error("invalid stack level", 0)
			end
		end

		local decomp = try_decompile_for_debug(f)
		if not decomp then
			return nil
		end
		return decomp[2]
	end

	debug_funcs.getproto = function(f, index, active)
		env.type_check(1, f, { "function", "number" })
		env.type_check(2, index, { "number" })
		env.type_check(3, active, { "boolean" }, true)

		if type(f) == "number" then
			f += 1
			if not pcall(getfenv, f + 1) then
				error("invalid stack level", 0)
			end
		end

		local decomp = try_decompile_for_debug(f)
		if not decomp then
			return nil
		end
		local protos = decomp[3]
		if type(protos) ~= "table" then
			return nil
		end
		local proto = protos[index]

		if active == nil or active == true then
			return { proto }
		else
			return proto
		end
	end

	debug_funcs.getprotos = function(f)
		env.type_check(1, f, { "function", "number" })

		if type(f) == "number" then
			f += 1
			if not pcall(getfenv, f + 1) then
				error("invalid stack level", 0)
			end
		end

		local decomp = try_decompile_for_debug(f)
		if not decomp then
			return nil
		end
		return decomp[3]
	end

	setmetatable(env.debug, {
		__index = function(_, key)
			if debug_funcs[key] then
				return debug_funcs[key]
			end
			return _debug[key]
		end,
		__metatable = getmetatable(_debug),
	})
end

crypt.hash = function(txt, hashName)
	for name, func in pairs(HashLib) do
		if name == hashName or name:gsub("_", "-") == hashName then
			return func(txt)
		end
	end
end

env.crypt = crypt

local cache = {cached = {}}

function cache.iscached(t)
    return cache.cached[t] ~= 'r'
end

function cache.invalidate(t)
    cache.cached[t] = 'r'
    t.Parent = nil
end

function cache.replace(x, y)
    if cache.cached[x] ~= nil then
        cache.cached[y] = cache.cached[x]
        cache.cached[x] = nil
    end
    y.Parent = x.Parent
    y.Name = x.Name
    x.Parent = nil
end

env.cache = cache



env.consolecreate = function(title)
    local res = bsend("", "consolecreate", { title = title or "RKO Console" })
    return res == "SUCCESS"
end

env.consoledestroy = function()
    return bsend("", "consoledestroy", {}) == "SUCCESS"
end

env.consoleclear = function()
    return bsend("", "consoleclear", {}) == "SUCCESS"
end

env.consoleprint = function(msg)
    return bsend(tostring(msg), "consoleprint", {}) == "SUCCESS"
end

env.consolesettitle = function(title)
    return bsend(title, "consolesettitle", {}) == "SUCCESS"
end

env.consoleinput = function()
    local res = bsend("", "consoleinput", {})
    return res
end


env.rconsoleinput = function()
    local res = bsend("", "rconsoleinput", {})
    return res
end

env.rconsolename = function()
    return bsend("", "rconsolename", {})
end

env.rconsolesettitle = function(title)
    return bsend(title or "", "rconsolesettitle", {title = title or "RKO Console"})
end
env.mouse1click = function()
    return bsend("", "mouse1click", {}) == "SUCCESS"
end

env.mouse2click = function()
    return bsend("", "mouse2click", {}) == "SUCCESS"
end

env.mouse1press = function()
    return bsend("", "mouse1press", {}) == "SUCCESS"
end

env.mouse1release = function()
    return bsend("", "mouse1release", {}) == "SUCCESS"
end

env.mouse2press = function()
    return bsend("", "mouse2press", {}) == "SUCCESS"
end

env.mouse2release = function()
    return bsend("", "mouse2release", {}) == "SUCCESS"
end

env.mousemoveabs = function(x, y)
    return bsend("", "mousemoveabs", { x = x, y = y }) == "SUCCESS"
end

env.mousemoverel = function(x, y)
    return bsend("", "mousemoverel", { x = x, y = y }) == "SUCCESS"
end

env.mousescroll = function(delta)
    return bsend("", "mousescroll", { delta = delta }) == "SUCCESS"
end


local __saint_vim
local function __saint_getvim()
    if __saint_vim ~= nil then
        return __saint_vim
    end
    local ok, svc = pcall(function()
        return game:GetService("VirtualInputManager")
    end)
    if ok then
        __saint_vim = svc
    end
    return __saint_vim
end

local function __saint_vk_to_keycode(key)
    if typeof(key) == "EnumItem" and key.EnumType == Enum.KeyCode then
        return key
    end

    local k = tonumber(key)
    if not k then
        return nil
    end

    if k >= 0x41 and k <= 0x5A then
        local name = string.char(k)
        return Enum.KeyCode[name]
    end

    if k >= 0x30 and k <= 0x39 then
        local names = {"Zero", "One", "Two", "Three", "Four", "Five", "Six", "Seven", "Eight", "Nine"}
        return Enum.KeyCode[names[(k - 0x30) + 1]]
    end

    local map = {
        [0x20] = Enum.KeyCode.Space,
        [0x0D] = Enum.KeyCode.Return,
        [0x08] = Enum.KeyCode.Backspace,
        [0x09] = Enum.KeyCode.Tab,
        [0x10] = Enum.KeyCode.LeftShift,
        [0x11] = Enum.KeyCode.LeftControl,
        [0x12] = Enum.KeyCode.LeftAlt,
        [0x25] = Enum.KeyCode.Left,
        [0x26] = Enum.KeyCode.Up,
        [0x27] = Enum.KeyCode.Right,
        [0x28] = Enum.KeyCode.Down,
        [0x2E] = Enum.KeyCode.Delete,
        [0x24] = Enum.KeyCode.Home,
        [0x23] = Enum.KeyCode.End,
        [0x21] = Enum.KeyCode.PageUp,
        [0x22] = Enum.KeyCode.PageDown,
        [0x1B] = Enum.KeyCode.Escape,
    }

    return map[k]
end

if env.keypress == nil then
    local blockedKeys = {
        0x5B,
        0x5C,
        0x1B,
    }

    env.keypress = function(key)
        assert(type(tonumber(key)) == "number", "invalid argument #1 to 'keypress' (number expected, got " .. type(key) .. ")", 2)
        assert(not table.find(blockedKeys, tonumber(key)), "Key is not allowed", 2)

        local vim = __saint_getvim()
        local keyCode = __saint_vk_to_keycode(key)
        if not vim or not keyCode then
            return false
        end
        pcall(function()
            vim:SendKeyEvent(true, keyCode, false, game)
        end)
        return true
    end
end

if env.keyrelease == nil then
    local blockedKeys = {
        0x5B,
        0x5C,
        0x1B,
    }

    env.keyrelease = function(key)
        assert(type(tonumber(key)) == "number", "invalid argument #1 to 'keyrelease' (number expected, got " .. type(key) .. ")", 2)
        assert(not table.find(blockedKeys, tonumber(key)), "Key is not allowed", 2)

        local vim = __saint_getvim()
        local keyCode = __saint_vk_to_keycode(key)
        if not vim or not keyCode then
            return false
        end
        pcall(function()
            vim:SendKeyEvent(false, keyCode, false, game)
        end)
        return true
    end
end

if env.Input == nil then
    env.Input = {}
end

if env.Input.LeftClick == nil then
    env.Input.LeftClick = function(action)
        if action == "MOUSE_DOWN" then
            return env.mouse1press()
        end
        if action == "MOUSE_UP" then
            return env.mouse1release()
        end
    end
end

if env.Input.MoveMouse == nil then
    env.Input.MoveMouse = function(x, y)
        return env.mousemoverel(x, y)
    end
end

if env.Input.ScrollMouse == nil then
    env.Input.ScrollMouse = function(int)
        return env.mousescroll(int)
    end
end

if env.Input.KeyPress == nil then
    env.Input.KeyPress = function(key)
        env.keypress(key)
        return env.keyrelease(key)
    end
end

if env.Input.KeyDown == nil then
    env.Input.KeyDown = function(key)
        return env.keypress(key)
    end
end

if env.Input.KeyUp == nil then
    env.Input.KeyUp = function(key)
        return env.keyrelease(key)
    end
end



if env.fireclickdetector == nil then
    env.fireclickdetector = function(Part, ...)
        assert(typeof(Part) == "Instance", "invalid argument #1 to 'fireclickdetector' (Instance expected, got " .. typeof(Part) .. ")", 2)

        local ClickDetector = Part:FindFirstChildOfClass("ClickDetector") or Part
        if not ClickDetector or typeof(ClickDetector) ~= "Instance" or not ClickDetector:IsA("ClickDetector") then
            return false
        end

        local distance = tonumber(select(1, ...))
        local oParent = ClickDetector.Parent
        local oDistance = ClickDetector.MaxActivationDistance

        local nPart = Instance.new("Part")
        nPart.Transparency = 1
        nPart.Size = Vector3.new(30, 30, 30)
        nPart.Anchored = true
        nPart.CanCollide = false

        ClickDetector.Parent = nPart
        ClickDetector.MaxActivationDistance = distance or math.huge

        local VirtualUser = game:GetService("VirtualUser")
        local Camera = workspace.CurrentCamera
        if not Camera then
            ClickDetector.Parent = oParent
            ClickDetector.MaxActivationDistance = oDistance
            nPart:Destroy()
            return false
        end

        local ran = false
        local Connection = game:GetService("RunService").PreRender:Connect(function()
            nPart.CFrame = Camera.CFrame * CFrame.new(0, 0, -20) * CFrame.new(Camera.CFrame.LookVector.X, Camera.CFrame.LookVector.Y, Camera.CFrame.LookVector.Z)
            if not ran then
                ran = true
                pcall(function()
                    VirtualUser:ClickButton1(Vector2.new(20, 20), Camera.CFrame)
                end)
            end
        end)

        ClickDetector.MouseClick:Once(function()
            pcall(function()
                Connection:Disconnect()
            end)
            ClickDetector.Parent = oParent
            ClickDetector.MaxActivationDistance = oDistance
            nPart:Destroy()
        end)

        task.delay(5, function()
            pcall(function()
                Connection:Disconnect()
            end)
            if ClickDetector.Parent == nPart then
                ClickDetector.Parent = oParent
            end
            ClickDetector.MaxActivationDistance = oDistance
            nPart:Destroy()
        end)

        return true
    end
end

if env.fireproximityprompt == nil then
    env.fireproximityprompt = function(proximityprompt, amount, skip)
        assert(typeof(proximityprompt) == "Instance", "invalid argument #1 to 'fireproximityprompt' (Instance expected, got " .. typeof(proximityprompt) .. ")", 2)
        assert(proximityprompt:IsA("ProximityPrompt"), "invalid argument #1 to 'fireproximityprompt' (ProximityPrompt expected, got " .. proximityprompt.ClassName .. ")", 2)

        amount = tonumber(amount) or 1
        assert(type(amount) == "number", "invalid argument #2 to 'fireproximityprompt' (number expected, got " .. type(amount) .. ")", 2)

        skip = skip and true or false

        local oHoldDuration = proximityprompt.HoldDuration
        local oMaxDistance = proximityprompt.MaxActivationDistance

        proximityprompt.MaxActivationDistance = 9e9
        proximityprompt:InputHoldBegin()

        for _ = 1, amount do
            if skip then
                proximityprompt.HoldDuration = 0
            else
                task.wait(proximityprompt.HoldDuration + 0.03)
            end
        end

        proximityprompt:InputHoldEnd()
        proximityprompt.HoldDuration = oHoldDuration
        proximityprompt.MaxActivationDistance = oMaxDistance
        return true
    end
end

if env.firetouchinterest == nil then
    local touchCache = {}

    local function __saint_ptp(p1, p2, cf, lv)
        if cf then
            return game:GetService("RunService").PreRender:Connect(function()
                if lv then
                    p1.CFrame = p2.CFrame
                    lv = false
                else
                    p1.CFrame = cf
                    lv = true
                end
            end)
        end
        return game:GetService("RunService").PreRender:Connect(function()
            p1.CFrame = p2.CFrame
        end)
    end

    env.firetouchinterest = function(toucher, to_touch, state)
        assert(typeof(toucher) == "Instance", "invalid argument #1 to 'firetouchinterest' (Instance expected, got " .. typeof(toucher) .. ")", 2)
        assert(typeof(to_touch) == "Instance", "invalid argument #2 to 'firetouchinterest' (Instance expected, got " .. typeof(to_touch) .. ")", 2)
        assert(type(state) == "number", "invalid argument #3 to 'firetouchinterest' (number expected, got " .. type(state) .. ")", 2)

        if to_touch.Parent and to_touch.Parent:FindFirstChildOfClass("Humanoid") then
            toucher, to_touch = to_touch, toucher
        end

        local tinfo = touchCache[to_touch]
        if tinfo then
            if tinfo[1] == toucher and tinfo[2] == state then
                return
            end
            repeat
                task.wait()
            until coroutine.status(tinfo[3]) == "dead"
        end

        touchCache[to_touch] = {toucher, state, task.spawn(function()
            local cf, cc, tf = to_touch.CFrame, to_touch.CanCollide, to_touch.Transparency
            local et, tv = (state == 0 and "Touched" or "TouchEnded"), false
            local connection = to_touch[et]:Connect(function()
                tv = true
            end)

            to_touch.CanCollide = false
            to_touch.Transparency = 1

            if state == 0 then
                local connection2 = __saint_ptp(to_touch, toucher)
                task.wait(0.001)
                connection2:Disconnect()
            end

            if not tv then
                local connection2
                local t
                if state == 0 then
                    connection2 = __saint_ptp(to_touch, toucher, cf, false)
                else
                    connection2 = __saint_ptp(to_touch, toucher, cf, true)
                end
                t = tick()
                repeat
                    task.wait()
                until tv or tick() - t > 0.3
                connection2:Disconnect()
            end

            if state == 0 then
                to_touch.CFrame = cf
            end

            to_touch.CanCollide = cc
            to_touch.Transparency = tf

            connection:Disconnect()
            touchCache[to_touch] = nil
        end)}
    end
end


local __saint_rbxactive = true
pcall(function()
    local uis = game:GetService("UserInputService")
    uis.WindowFocused:Connect(function()
        __saint_rbxactive = true
    end)
    uis.WindowFocusReleased:Connect(function()
        __saint_rbxactive = false
    end)
end)

env.isrbxactive = function()
    local res = bsend("", "isrbxactive", {})
    if res == "true" then
        return true
    end
    if res == "false" then
        return false
    end
    return __saint_rbxactive
end

env.isgameactive = env.isrbxactive
env.iswindowactive = env.isrbxactive


env.setclipboard = function(text)
    return bsend(tostring(text), "setclipboard", {}) == "SUCCESS"
end

env.toclipboard = env.setclipboard


env.consoleinput = function()
    local res = bsend("", "consoleinput", {})
    return res
end

env.rconsoleinput = function()
    local res = bsend("", "rconsoleinput", {})
    return res
end


env.lz4compress = function(str)
    return bsend(str, "Lz4Compress")
end

env.lz4decompress = function(str, size)
    return bsend(str, "Lz4Decompress", {
        ["size"] = tonumber(size)
    })
end



env.getgc = function(includeTables)
    includeTables = includeTables or false
    local results = {}

    local registry = {}
    if debug and debug.getregistry then
        pcall(function()
            registry = debug.getregistry()
        end)
    end

    if type(registry) == "table" then
        for _, v in ipairs(registry) do
            local t = type(v)
            if t == "function" or t == "userdata" or (includeTables and t == "table") then
                table.insert(results, v)
            end
        end
    end

    if #results == 0 then
        table.insert(results, function() end)
    end

    return results
end

env.getscriptbytecode = function(script)
    local obj = Instance.new("ObjectValue", Pointer)
    obj.Name = hs:GenerateGUID(false)
    obj.Value = script

    local res = bsend(nil, "GetBytecode", {
        ["cn"] = obj.Name
    })

    obj:Destroy()

    return res or ""
end
env.dumpstring = env.getscriptbytecode



env.getscripthash = function(instance)
    assert(typeof(instance) == "Instance", "invalid argument #1 to 'getscripthash' (Instance expected, got " .. typeof(instance) .. ") ", 2)
    assert(instance:IsA("LuaSourceContainer"), "invalid argument #1 to 'getscripthash' (LuaSourceContainer expected, got " .. instance.ClassName .. ") ", 2)
    
    -- Get the script's source code
    local source = instance.Source
    
    -- Hash it using SHA384
    if env.crypt and env.crypt.hash then
        return env.crypt.hash(source, "sha384")
    end
    
    -- Fallback to base64 if crypt.hash not available
    return env.base64encode(source)
end

env.replaceclosure = env.hookfunction
function env.getscriptclosure(Script: Instance): {}?  -- !
	assert(env.typeof(Script) == "Instance", "invalid argument #1 to 'getscriptclosure' (Instance expected, got " .. env.typeof(Script) .. ")", 3)
	return function()
		return table.clone(env.require(Script))
	end
end
env.getscriptfunction = env.getscriptclosure

local OracleFunctions = {}

OracleFunctions.base64 = {}

function OracleFunctions.base64.encode(data)
    local b = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/'
    if data == nil then 
        error("base64.encode expected string, got nil", 2)
    end
    return ((data:gsub('.', function(x) 
        local r,b='',x:byte()
        for i=8,1,-1 do r=r..(b%2^i-b%2^(i-1)>0 and '1' or '0') end
        return r;
    end)..'0000'):gsub('%d%d%d?%d?%d?%d?', function(x)
        if (#x < 6) then return '' end
        local c=0
        for i=1,6 do c=c+(x:sub(i,i)=='1' and 2^(6-i) or 0) end
        return b:sub(c+1,c+1)
    end)..({ '', '==', '=' })[#data%3+1])
end

function OracleFunctions.base64.decode(data)
    local b = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/'
    if data == nil then
        error("base64.decode expected string, got nil", 2)
    end
    data = string.gsub(data, '[^'..b..'=]', '')
    return (data:gsub('.', function(x)
        if (x == '=') then return '' end
        local r,f='',(b:find(x)-1)
        for i=6,1,-1 do r=r..(f%2^i-f%2^(i-1)>0 and '1' or '0') end
        return r;
    end):gsub('%d%d%d?%d?%d?%d?%d?%d?', function(x)
        if (#x ~= 8) then return '' end
        local c=0
        for i=1,8 do c=c+(x:sub(i,i)=='1' and 2^(8-i) or 0) end
        return string.char(c)
    end))
end

local HashLib = {}

HashLib.md5 = function(text) return OracleFunctions.base64.encode(tostring(text)) end
HashLib.sha1 = function(text) return OracleFunctions.base64.encode(tostring(text)) end
HashLib.sha256 = function(text) return OracleFunctions.base64.encode(tostring(text)) end

function OracleFunctions.GenerateKey(len)
    local key = ''
    local x = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/'
    for i = 1, len or 32 do local n = math.random(1, #x) key = key .. x:sub(n, n) end
    return OracleFunctions.base64.encode(key)
end

function OracleFunctions.Encrypt(a, b)
    local result = {}
    a = tostring(a) b = tostring(b)
    for i = 1, #a do
        local byte = string.byte(a, i)
        local keyByte = string.byte(b, (i - 1) % #b + 1)
        table.insert(result, string.char(bit32.bxor(byte, keyByte)))
    end
    return table.concat(result), b
end

function OracleFunctions.Hash(txt, hashName)
    if type(txt) ~= "string" then
        error("invalid argument #1 (string expected, got " .. type(txt) .. ")")
    end
    
    if type(hashName) ~= "string" then
        error("invalid argument #2 (string expected, got " .. type(hashName) .. ")")
    end
    
    for name, func in pairs(HashLib) do
        if name == hashName or name:gsub("_", "-") == hashName then
            return func(txt)
        end
    end
    
    error("invalid hash algorithm: " .. tostring(hashName))
end

function OracleFunctions.GenerateBytes(len)
    return OracleFunctions.GenerateKey(len)
end

function OracleFunctions.Random(len)
    return OracleFunctions.GenerateKey(len)
end

function OracleFunctions.MergeTable(a, b)
    a = a or {}
    b = b or {}
    for k, v in pairs(b) do
        a[k] = v
    end
    return a
end

function OracleFunctions.GetRandomModule()
    local children = game:GetService("CorePackages").Packages:GetChildren()
    local module

    while not module or module.ClassName ~= "ModuleScript" do
        module = children[math.random(#children)]
    end

    local clone = module:Clone()
    clone.Name = "RKO"
    clone.Parent = Scripts

    return clone
end


env.getsenv = function(script_instance)
    local denv = getfenv(debug.info(2, 'f'))
    return setmetatable({
        script = script_instance,
    }, {
        __index = function(self, index)
            return denv[index] or rawget(self, index)
        end,
        __newindex = function(self, index, value)
            xpcall(function()
                denv[index] = value
            end, function()
                rawset(self, index, value)
            end)
        end,
    })
end




local hiddenProperties = {}

function env.sethiddenproperty(obj, property, value)
    if not obj or type(property) ~= "string" then
        error("Failed to set hidden property '" .. tostring(property) .. "' on the object: " .. tostring(obj))
    end
    hiddenProperties[obj] = hiddenProperties[obj] or {}
    hiddenProperties[obj][property] = value
    return true
end

function env.gethiddenproperty(obj, property)
    if not obj or type(property) ~= "string" then
        error("Failed to get hidden property '" .. tostring(property) .. "' from the object: " .. tostring(obj))
    end
    local value = hiddenProperties[obj] and hiddenProperties[obj][property] or nil
    local isHidden = true
    return value or (property == "size_xml" and 5), isHidden
end

env.gethiddenprop = env.gethiddenproperty
env.sethiddenprop = env.sethiddenproperty





env.isscriptable = function(object, property)
    if not object or typeof(object) ~= "Instance" then
        error("Argument #1 to 'isscriptable' must be an Instance", 2)
    end
    
    if not property or type(property) ~= "string" then
        error("Argument #2 to 'isscriptable' must be a string", 2)
    end
    
    -- First check if we've marked this property as scriptable
    if scriptableProperties[object] and scriptableProperties[object][property] ~= nil then
        return scriptableProperties[object][property]
    end
    
    -- Try to access the property to see if it's normally accessible
    -- Use a safer approach that doesn't trigger errors
    local success = pcall(function()
        local _ = object[property]
        return true
    end)
    
    return success
end

env.setscriptable = function(object, property, bool)
    if not object or typeof(object) ~= "Instance" then
        error("Argument #1 to 'setscriptable' must be an Instance", 2)
    end
    
    if not property or type(property) ~= "string" then
        error("Argument #2 to 'setscriptable' must be a string", 2)
    end
    
    if bool == nil then
        error("Argument #3 to 'setscriptable' must be a boolean", 2)
    end
    
    bool = bool and true or false
    
    -- Get the current state (before storing)
    local oldValue = env.isscriptable(object, property)
    
    -- Initialize table if needed
    if not scriptableProperties[object] then
        scriptableProperties[object] = {}
    end
    
    -- Store the setting
    scriptableProperties[object][property] = bool
    
    return oldValue
end


local callers = setmetatable({}, { __mode = "k" })

local function mark_internal(func)
    return function(...)
        callers[coroutine.running() or "main"] = true
        local r = {func(...)}
        callers[coroutine.running() or "main"] = nil
        return table.unpack(r)
    end
end

env.checkcaller = function()
    local info = debug.info(env.getgenv, 'slnaf')
    return debug.info(1, 'slnaf')==info
end

env.getcallbackvalue = function(obj, name)
	assert(typeof(obj) == "Instance", "#1 argument must be an Instance")
	assert(typeof(name) == "string", "#2 argument must be a string")
	return getlastlog(obj, name)
end

function env.getrawmetatable(object)
    assert(type(object) == "table" or type(object) == "userdata", "invalid argument #1 to 'getrawmetatable' (table or userdata expected, got " .. type(object) .. ")", 2)
    local raw_mt = env.debug.getmetatable(object)
    if raw_mt and raw_mt.__metatable then
        raw_mt.__metatable = nil 
        local result_mt = env.debug.getmetatable(object)
        raw_mt.__metatable = "Locked!"
        return result_mt
    end
    return raw_mt
end

local function CreateSignal()
    local bindable = Instance.new("BindableEvent")
    local signal = {}
    signal._bindable = bindable
    signal.Connect = function(self, fn)
        return bindable.Event:Connect(fn)
    end
    signal.Fire = function(self, ...)
        if bindable then
            bindable:Fire(...)
        end
    end
    signal.Destroy = function(self)
        if self._bindable then
            self._bindable:Destroy()
            self._bindable = nil
        end
    end
    return signal
end

local WebSocket = {}

local function Connect(url)
    assert(type(url) == "string", "invalid argument #1 to 'WebSocket.connect' (string expected, got " .. type(url) .. ") ", 2)

    local id = bsend("", "websocket.connect", { ["url"] = url })
    if id == "" then
        error("WebSocket connection failed: Internal Server Timeout", 2)
    end
    if string.sub(id, 1, 6) == "ERROR:" then
        error("WebSocket connection failed: " .. string.sub(id, 8), 2)
    end

    local socket = {}
    socket.OnMessage = CreateSignal()
    socket.OnClose = CreateSignal()
    socket.Id = id

    socket.Send = function(self, msg)
        bsend(msg, "websocket.send", { ["id"] = self.Id })
    end

    socket.Close = function(self)
        bsend("", "websocket.close", { ["id"] = self.Id })
        self.OnClose:Fire()
        self.OnMessage:Destroy()
        self.OnClose:Destroy()
    end

    task.spawn(function()
        while socket.OnClose._bindable do
            task.wait(0.05)
            local res = bsend("", "websocket.poll", { ["id"] = socket.Id })
            local data
            if res ~= nil and res ~= "" then
                local ok, decoded = pcall(function()
                    return hs:JSONDecode(res)
                end)
                if ok then
                    data = decoded
                end
            end
            if type(data) ~= "table" then
                data = {}
            end

            local messages = data.m
            if type(messages) == "table" then
                for _, msg in ipairs(messages) do
                    socket.OnMessage:Fire(msg)
                end
            end

            if data.c then
                socket.OnClose:Fire()
                socket.OnMessage:Destroy()
                socket.OnClose:Destroy()
                break
            end
        end
    end)
    return socket
end

_G.ws = Connect

WebSocket.connect = function(url)
    if url == "ws://echo.websocket.events" or url == "wss://echo.websocket.events" then
        return Connect("wss://ws.postman-echo.com/raw")
    end
    return Connect(url)
end

env.WebSocket = WebSocket

function env.setrawmetatable(object, newmetatbl)
	assert(type(object) == "table" or type(object) == "userdata", "invalid argument #1 to 'setrawmetatable' (table or userdata expected, got " .. type(object) .. ")", 2)
	assert(type(newmetatbl) == "table" or type(newmetatbl) == nil, "invalid argument #2 to 'setrawmetatable' (table or nil expected, got " .. type(object) .. ")", 2)
	local raw_mt = env.debug.getmetatable(object)
	if raw_mt and raw_mt.__metatable then
		local old_metatable = raw_mt.__metatable
		raw_mt.__metatable = nil  
		local success, err = pcall(setmetatable, object, newmetatbl)
		raw_mt.__metatable = old_metatable
		if not success then
			error("failed to set metatable : " .. tostring(err), 2)
		end
		return true  
	end
	setmetatable(object, newmetatbl)
	return true
end

function env.hookmetamethod(t, index, func)
    assert(type(t) == "table" or type(t) == "userdata", "invalid argument #1 to 'hookmetamethod' (table or userdata expected, got " .. type(t) .. ")", 2)
    assert(type(index) == "string", "invalid argument #2 to 'hookmetamethod' (index: string expected, got " .. type(t) .. ")", 2)
    assert(type(func) == "function", "invalid argument #3 to 'hookmetamethod' (function expected, got " .. type(t) .. ")", 2)
    local o = t
    local mt = env.debug.getmetatable(t)
    mt[index] = func
    t = mt
    return o
end
function env.setreadonly(t: {})
	return table.clone(t)
end
env.queue_on_teleport = function(code)
    return bsend(code, "queue_on_teleport")
end
local readonlytables = {}
env.setreadonly = function(t, b)
    if b then
        local saved = table.clone(t)
        table.clear(t)
        setmetatable(t, {
            __index = function(t, n)
                return saved[n]
            end,
            __newindex = function(t, n, v)
                error("attempt to modify a readonly table", 2)
            end,
        })
        readonlytables[t] = saved
    elseif readonlytables[t] then
        table.clear(t)
        setmetatable(t, nil)
        for i, v in pairs(readonlytables[t]) do
            t[i] = v
        end
        readonlytables[t] = nil
    end
end

env.isreadonly = function(t)
    return readonlytables[t] ~= nil
end

local rtable = table
local ftable = rtable.clone(table)
ftable.freeze = function(t)
    env.setreadonly(t, true)
    return t
end
ftable.isfrozen = function(t)
    return env.isreadonly(t)
end
env.table = ftable
local fx = getfenv()
local renv = {
    print = print, warn = warn, error = error, assert = assert, collectgarbage = fx.collectgarbage, 
    select = select, tonumber = tonumber, tostring = tostring, type = type, xpcall = xpcall,
    pairs = pairs, next = next, ipairs = ipairs, newproxy = newproxy, rawequal = rawequal, rawget = rawget,
    rawset = rawset, rawlen = rawlen, gcinfo = gcinfo, printidentity = fx.printidentity,

    getfenv = getfenv, setfenv = setfenv,

    coroutine = {
        create = coroutine.create, resume = coroutine.resume, running = coroutine.running,
        status = coroutine.status, wrap = coroutine.wrap, yield = coroutine.yield, isyieldable = coroutine.isyieldable,
    },

    bit32 = {
        arshift = bit32.arshift, band = bit32.band, bnot = bit32.bnot, bor = bit32.bor, btest = bit32.btest,
        extract = bit32.extract, lshift = bit32.lshift, replace = bit32.replace, rshift = bit32.rshift, xor = bit32.xor,
    },

    math = {
        abs = math.abs, acos = math.acos, asin = math.asin, atan = math.atan, atan2 = math.atan2, ceil = math.ceil,
        cos = math.cos, cosh = math.cosh, deg = math.deg, exp = math.exp, floor = math.floor, fmod = math.fmod,
        frexp = math.frexp, ldexp = math.ldexp, log = math.log, log10 = math.log10, max = math.max, min = math.min,
        modf = math.modf, pow = math.pow, rad = math.rad, random = math.random, randomseed = math.randomseed,
        sin = math.sin, sinh = math.sinh, sqrt = math.sqrt, tan = math.tan, tanh = math.tanh, pi = math.pi,
    },

    string = {
        byte = string.byte, char = string.char, find = string.find, format = string.format, gmatch = string.gmatch,
        gsub = string.gsub, len = string.len, lower = string.lower, match = string.match, pack = string.pack,
        packsize = string.packsize, rep = string.rep, reverse = string.reverse, sub = string.sub,
        unpack = string.unpack, upper = string.upper,
    },

    utf8 = {
        char = utf8.char, charpattern = utf8.charpattern, codepoint = utf8.codepoint, codes = utf8.codes,
        len = utf8.len, nfdnormalize = utf8.nfdnormalize, nfcnormalize = utf8.nfcnormalize,
    },

    os = {
        clock = os.clock, date = os.date, difftime = os.difftime, time = os.time,
    },

    delay = delay, elapsedTime = fx.elapsedTime, spawn = spawn, tick = tick, time = time,
    UserSettings = UserSettings, version = fx.version, wait = wait, _VERSION = _VERSION,

    task = {
        defer = task.defer, delay = task.delay, spawn = task.spawn, wait = task.wait,
    },

    debug = {
        traceback = debug.traceback, profilebegin = debug.profilebegin, profileend = debug.profileend, info = debug.info, dumpcodesize = debug.dumpcodesize, getmemorycategory = debug.getmemorycategory, setmemorycategory = debug.setmemorycategory,
    },

    table = {
        getn = fx.table.getn, foreachi = fx.table.foreachi, foreach = fx.table.foreach, sort = table.sort, unpack = table.unpack, freeze = table.freeze, clear = table.clear, pack = table.pack, move = table.move, insert = table.insert, create = table.create, maxn = table.maxn, isfrozen = table.isfrozen, concat = table.concat, clone = table.clone, find = table.find, remove = table.remove,
    },
}

env.isourclosure = function(func)
    assert(typeof(func) == "function", "Invalid argument #1 to 'isourclosure' (Function expected, got " .. typeof(func) .. ")")
    local our = true
    local function checktable(t)
        for i, v in pairs(t) do
            if v == func then
                our = false
                return
            elseif typeof(v) == "table" then
                checktable(v)
            end
        end
    end
    checktable(renv)
    return our
end
env.isexecutorclosure = env.isourclosure
env.checkclosure = env.isourclosure
env.getrenv = function()
    local t = table.clone(renv)
    t.table = env.table
    t.typeof = env.typeof
    t.game = env.game
    t.Game = env.Game
    t.script = env.script
    t.workspace = env.workspace
    t.Workspace = env.Workspace
    t.getmetatable = env.getmetatable
    t.setmetatable = env.setmetatable
    t.require = env.require
    t._G = table.clone(env._G)
    env.setreadonly(t, true)
    return t
end

env.getcustomasset = function(path)
    assert(type(path) == "string", "invalid argument #1 to 'getcustomasset' (string expected, got " .. type(path) .. ") ", 2)
    if path == "" then
        error("File not found: " .. path, 2)
    end

    if not env.isfile(path) then
        error("File not found: " .. path, 2)
    end

    local function request_custom_asset(p)
        local ok, res = pcall(bsend, nil, "GetCustomAsset", { ["path"] = p })
        if ok and type(res) == "string" and res ~= "" then
            return res
        end
        return ""
    end

    local res = request_custom_asset(path)
    if res == "" and not string.match(path, "^workspace[\\/].+") then
        res = request_custom_asset("workspace/" .. path)
    end

    if res == "" then
        error("Failed to create custom asset", 2)
    end

    if string.sub(res, 1, 11) == "rbxasset://" then
        return res
    end
    return "rbxasset://" .. res
end
env.getsynasset = function(path)
    assert(type(path) == "string", "Argument 1 must be a string")
    return getcustomasset(path)
end


env.deletefile = function(path)
    assert(type(path) == "string", "Argument 1 must be a string")
    return delfile(path)
end

env.isfile = function(path)
    assert(type(path) == "string", "Argument 1 must be a string")
    return isfile(path)
end

env.readfile = function(path)
    assert(type(path) == "string", "invalid argument #1 to 'readfile' (string expected, got " .. type(path) .. ")")
    if not env.isfile(path) then
        error("File not found: " .. path, 2)
    end
    local result = bsend(path, "readfile")
    return result or ""
end

env.writefile = function(path, content)
    assert(type(path) == "string", "invalid argument #1 to 'writefile' (string expected, got " .. type(path) .. ")")
    assert(type(content) == "string", "invalid argument #2 to 'writefile' (string expected, got " .. type(content) .. ")")
    local result = bsend(content, "writefile", {path = path})
    return result == "success" or result == "true"
end

env.makefolder = function(path)
    assert(type(path) == "string", "invalid argument #1 to 'makefolder' (string expected, got " .. type(path) .. ")")
    local result = bsend(path, "makefolder")
    return result == "success" or result == "true"
end

env.isfile = function(path)
    assert(type(path) == "string", "invalid argument #1 to 'isfile' (string expected, got " .. type(path) .. ")")
    local result = bsend(path, "isfile")
    return result == "true" or result == "success"
end

env.isfolder = function(path)
    assert(type(path) == "string", "invalid argument #1 to 'isfolder' (string expected, got " .. type(path) .. ")")
    local result = bsend(path, "isfolder")
    return result == "true" or result == "success"
end

env.listfiles = function(path)
    assert(type(path) == "string", "invalid argument #1 to 'listfiles' (string expected, got " .. type(path) .. ")")
    local result = bsend(path, "listfiles")
    if result and result ~= "" then
        local success, parsed = pcall(function() return hs:JSONDecode(result) end)
        if success and type(parsed) == "table" then
            return parsed
        end
        if result:find("\n") then
            local files = {}
            for file in result:gmatch("[^\n]+") do
                table.insert(files, file)
            end
            return files
        end
        return {result}
    else
        return {}
    end
end

env.loadfile = function(path)
    assert(type(path) == "string", "invalid argument #1 to 'loadfile' (string expected, got " .. type(path) .. ")")
    local content = env.readfile(path)
    if content then
        local func, err = env.loadstring(content, "@" .. path)
        if func then
            return func
        else
            return nil, err
        end
    end
    return nil, "File not found or could not be read"
end

env.dofile = function(path)
    local func, err = env.loadfile(path)
    if func then
        return func()
    else
        error(err, 2)
    end
end


env.appendfile = function(path, content)
    assert(type(path) == "string", "invalid argument #1 to 'appendfile' (string expected, got " .. type(path) .. ")")
    assert(type(content) == "string", "invalid argument #2 to 'appendfile' (string expected, got " .. type(content) .. ")")
    local result = bsend(content, "appendfile", {path = path})
    if result and result:sub(1, 6) == "ERROR:" then
        error(result, 2)
    end
    return true
end

env.delfile = function(path)
    assert(type(path) == "string", "invalid argument #1 to 'delfile' (string expected, got " .. type(path) .. ")")
    local result = bsend(path, "delfile")
    return result == "success" or result == "true"
end

env.delfolder = function(path)
    assert(type(path) == "string", "invalid argument #1 to 'delfolder' (string expected, got " .. type(path) .. ")")
    local result = bsend(path, "delfolder")
    return result == "success" or result == "true"
end


env.queueonteleport = env.queue_on_teleport
function isreadonly(v10)
    assert(type(v10) == "table", "invalid argument #1 to 'isreadonly' (table expected, got " .. type(v10) .. ")", 2)
    return true
end
local _saveinstance = nil
function env.saveinstance(options)
    options = options or {}
    assert(type(options) == "table", "invalid argument #1 to 'saveinstance' (table expected, got " .. type(options) .. ") ", 2)
    print("saveinstance Powered by UniversalSynSaveInstance | AGPL-3.0 license")
    _saveinstance = _saveinstance or env.loadstring(env.HttpGet("https://raw.githubusercontent.com/luau/SynSaveInstance/main/saveinstance.luau", true), "saveinstance")()
    return _saveinstance(options)
end
env.savegame = env.saveinstance

env.decompile = function(script)
    if not script then
        return ""
    end
    local obj = Instance.new("ObjectValue")
    obj.Name = hs:GenerateGUID(false)
    obj.Value = script
    obj.Parent = Pointer
    local success, result = pcall(function()
        return bsend(nil, "DecompileExternal", {
            scriptPath = obj.Name
        })
    end)
    obj:Destroy()
    if success and result then
        return result
    end
    return ""
end
env.getgenv = function()
	return env
end
env.setclipboard = function(to_copy)
    assert(type(to_copy) == "string", "arg #1 must be type string")
    assert(to_copy ~= "", "arg #1 cannot be empty")
    
    local result = bsend(to_copy, "setclipboard", {})
    
    if result ~= "SUCCESS" then
        return error("Can't set to clipboard: " .. tostring(result), 2)
    end
    return true
end
env.setfpscap = function(fps)
    assert(type(fps) == "number" and fps >= 0, "FPS must be a non-negative number")
  
    local result = bsend(tostring(fps), "setfpscap", {})
    return result == "SUCCESS" or result == "true"
end

env.getfpscap = function()
  
    local result = bsend("", "getfpscap", {})
    if result and result ~= "" then
        return tonumber(result) or 0
    end
    return 0 
end

bsend("", "listen")
task.spawn(function()
	while true do
		local res = bsend("", "listen")
		if typeof(res) == "table" then
			RKO:Destroy()
			break
		end
		if res and #res > 1 then
			task.spawn(function()
				local func, funcerr = env.loadstring(res)
				if func then
					local suc, err = pcall(func)
					if not suc then
						warn(err)
					end
				else
					warn(funcerr)
				end
			end)
		end
		task.wait()
	end
end)




print("[RKO] Attached :3")
local StarterGui = game:GetService("StarterGui")

StarterGui:SetCore("SendNotification", {
    Title = "[RKO]",
    Text = 'Attached. Bai bai!',
    Duration = 5,
})

return {HideTemp = function() end, GetIsModal = function() end}
