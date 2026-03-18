#pragma once

#include "Bridge.hpp"



#include <iostream>

#include <Windows.h>

#include <string>

#include <vector>

#include <thread>

#pragma comment(lib, "urlmon.lib")

#include <urlmon.h>



#include "Utils/Process.hpp"

#include "Utils/Instance.hpp"

#include "Utils/Bytecode.hpp"


const char g_DrawingLibUA[] =
R"__DRAWINGUA__(local coreGui = game:GetService("CoreGui")

local camera = workspace.CurrentCamera
local drawingUI = Instance.new("ScreenGui")
drawingUI.Name = "Drawing | Xeno"
drawingUI.IgnoreGuiInset = true
drawingUI.DisplayOrder = 0x7fffffff
drawingUI.Parent = coreGui

local drawingIndex = 0
local drawingFontsEnum = {
	[0] = Font.fromEnum(Enum.Font.Roboto),
	[1] = Font.fromEnum(Enum.Font.Legacy),
	[2] = Font.fromEnum(Enum.Font.SourceSans),
	[3] = Font.fromEnum(Enum.Font.RobotoMono)
}

local function getFontFromIndex(fontIndex)
	return drawingFontsEnum[fontIndex]
end

local function convertTransparency(transparency)
	return math.clamp(1 - transparency, 0, 1)
end

local baseDrawingObj = setmetatable({
	Visible = true,
	ZIndex = 0,
	Transparency = 1,
	Color = Color3.new(),
	Remove = function(self)
		setmetatable(self, nil)
	end,
	Destroy = function(self)
		setmetatable(self, nil)
	end,
	SetProperty = function(self, index, value)
		if self[index] ~= nil then
			self[index] = value
		else
			warn("Attempted to set invalid property: " .. tostring(index))
		end
	end,
	GetProperty = function(self, index)
		if self[index] ~= nil then
			return self[index]
		else
			warn("Attempted to get invalid property: " .. tostring(index))
			return nil
		end
	end,
	SetParent = function(self, parent)
		self.Parent = parent
	end
}, {
	__add = function(t1, t2)
		local result = {}
		for index, value in pairs(t1) do
			result[index] = value
		end
		for index, value in pairs(t2) do
			result[index] = value
		end
		return result
	end
})

local DrawingLib = {}
DrawingLib.Fonts = {
	["UI"] = 0,
	["System"] = 1,
	["Plex"] = 2,
	["Monospace"] = 3
}

function DrawingLib.new(drawingType)
	drawingIndex += 1
	if drawingType == "Line" then
		return DrawingLib.createLine()
	elseif drawingType == "Text" then
		return DrawingLib.createText()
	elseif drawingType == "Circle" then
		return DrawingLib.createCircle()
	elseif drawingType == "Square" then
		return DrawingLib.createSquare()
	elseif drawingType == "Image" then
		return DrawingLib.createImage()
	elseif drawingType == "Quad" then
		return DrawingLib.createQuad()
	elseif drawingType == "Triangle" then
		return DrawingLib.createTriangle()
	elseif drawingType == "Frame" then
		return DrawingLib.createFrame()
	elseif drawingType == "ScreenGui" then
		return DrawingLib.createScreenGui()
	elseif drawingType == "TextButton" then
		return DrawingLib.createTextButton()
	elseif drawingType == "TextLabel" then
		return DrawingLib.createTextLabel()
	elseif drawingType == "TextBox" then
		return DrawingLib.createTextBox()
	else
		error("Invalid drawing type: " .. tostring(drawingType))
	end
end

function DrawingLib.createLine()
	local lineObj = ({
		From = Vector2.zero,
		To = Vector2.zero,
		Thickness = 1
	} + baseDrawingObj)

	local lineFrame = Instance.new("Frame")
	lineFrame.Name = drawingIndex
	lineFrame.AnchorPoint = Vector2.new(0.5, 0.5)
	lineFrame.BorderSizePixel = 0

	lineFrame.Parent = drawingUI
	return setmetatable({Parent = drawingUI}, {
		__newindex = function(_, index, value)
			if lineObj[index] == nil then 
				warn("Invalid property: " .. tostring(index))
				return 
			end

			if index == "From" or index == "To" then
				local direction = (index == "From" and lineObj.To or value) - (index == "From" and value or lineObj.From)
				local center = (lineObj.To + lineObj.From) / 2
				local distance = direction.Magnitude
				local theta = math.deg(math.atan2(direction.Y, direction.X))

				lineFrame.Position = UDim2.fromOffset(center.X, center.Y)
				lineFrame.Rotation = theta
				lineFrame.Size = UDim2.fromOffset(distance, lineObj.Thickness)
			elseif index == "Thickness" then
				lineFrame.Size = UDim2.fromOffset((lineObj.To - lineObj.From).Magnitude, value)
			elseif index == "Visible" then
				lineFrame.Visible = value
			elseif index == "ZIndex" then
				lineFrame.ZIndex = value
			elseif index == "Transparency" then
				lineFrame.BackgroundTransparency = convertTransparency(value)
			elseif index == "Color" then
				lineFrame.BackgroundColor3 = value
			elseif index == "Parent" then
				lineFrame.Parent = value
			end
			lineObj[index] = value
		end,
		__index = function(self, index)
			if index == "Remove" or index == "Destroy" then
				return function()
					lineFrame:Destroy()
					lineObj:Remove()
				end
			end
			return lineObj[index]
		end,
		__tostring = function() return "Drawing" end
	})
end

function DrawingLib.createText()
	local textObj = ({
		Text = "",
		Font = DrawingLib.Fonts.UI,
		Size = 0,
		Position = Vector2.zero,
		Center = false,
		Outline = false,
		OutlineColor = Color3.new()
	} + baseDrawingObj)

	local textLabel, uiStroke = Instance.new("TextLabel"), Instance.new("UIStroke")
	textLabel.Name = drawingIndex
	textLabel.AnchorPoint = Vector2.new(0.5, 0.5)
	textLabel.BorderSizePixel = 0
	textLabel.BackgroundTransparency = 1

	local function updateTextPosition()
		local textBounds = textLabel.TextBounds
		local offset = textBounds / 2
		textLabel.Size = UDim2.fromOffset(textBounds.X, textBounds.Y)
		textLabel.Position = UDim2.fromOffset(textObj.Position.X + (not textObj.Center and offset.X or 0), textObj.Position.Y + offset.Y)
	end

	textLabel:GetPropertyChangedSignal("TextBounds"):Connect(updateTextPosition)

	uiStroke.Thickness = 1
	uiStroke.Enabled = textObj.Outline
	uiStroke.Color = textObj.Color

	textLabel.Parent, uiStroke.Parent = drawingUI, textLabel

	return setmetatable({Parent = drawingUI}, {
		__newindex = function(_, index, value)
			if textObj[index] == nil then 
				warn("Invalid property: " .. tostring(index))
				return 
			end

			if index == "Text" then
				textLabel.Text = value
			elseif index == "Font" then
				textLabel.FontFace = getFontFromIndex(math.clamp(value, 0, 3))
			elseif index == "Size" then
				textLabel.TextSize = value
			elseif index == "Position" then
				updateTextPosition()
			elseif index == "Center" then
				textLabel.Position = UDim2.fromOffset((value and camera.ViewportSize / 2 or textObj.Position).X, textObj.Position.Y)
			elseif index == "Outline" then
				uiStroke.Enabled = value
			elseif index == "OutlineColor" then
				uiStroke.Color = value
			elseif index == "Visible" then
				textLabel.Visible = value
			elseif index == "ZIndex" then
				textLabel.ZIndex = value
			elseif index == "Transparency" then
				local transparency = convertTransparency(value)
				textLabel.TextTransparency = transparency
				uiStroke.Transparency = transparency
			elseif index == "Color" then
				textLabel.TextColor3 = value
			elseif index == "Parent" then
				textLabel.Parent = value
			end
			textObj[index] = value
		end,
		__index = function(self, index)
			if index == "Remove" or index == "Destroy" then
				return function()
					textLabel:Destroy()
					textObj:Remove()
				end
			elseif index == "TextBounds" then
				return textLabel.TextBounds
			end
			return textObj[index]
		end,
		__tostring = function() return "Drawing" end
	})
end

)__DRAWINGUA__"
R"__DRAWINGUA__(function DrawingLib.createCircle()
	local circleObj = ({
		Radius = 150,
		Position = Vector2.zero,
		Thickness = 0.7,
		Filled = false
	} + baseDrawingObj)

	local circleFrame, uiCorner, uiStroke = Instance.new("Frame"), Instance.new("UICorner"), Instance.new("UIStroke")
	circleFrame.Name = drawingIndex
	circleFrame.AnchorPoint = Vector2.new(0.5, 0.5)
	circleFrame.BorderSizePixel = 0

	uiCorner.CornerRadius = UDim.new(1, 0)
	circleFrame.Size = UDim2.fromOffset(circleObj.Radius, circleObj.Radius)
	uiStroke.Thickness = circleObj.Thickness
	uiStroke.ApplyStrokeMode = Enum.ApplyStrokeMode.Border

	circleFrame.Parent, uiCorner.Parent, uiStroke.Parent = drawingUI, circleFrame, circleFrame

	return setmetatable({Parent = drawingUI}, {
		__newindex = function(_, index, value)
			if circleObj[index] == nil then 
				warn("Invalid property: " .. tostring(index))
				return 
			end

			if index == "Radius" then
				local radius = value * 2
				circleFrame.Size = UDim2.fromOffset(radius, radius)
			elseif index == "Position" then
				circleFrame.Position = UDim2.fromOffset(value.X, value.Y)
			elseif index == "Thickness" then
				uiStroke.Thickness = math.clamp(value, 0.6, 0x7fffffff)
			elseif index == "Filled" then
				circleFrame.BackgroundTransparency = value and convertTransparency(circleObj.Transparency) or 1
				uiStroke.Enabled = not value
			elseif index == "Visible" then
				circleFrame.Visible = value
			elseif index == "ZIndex" then
				circleFrame.ZIndex = value
			elseif index == "Transparency" then
				local transparency = convertTransparency(value)
				circleFrame.BackgroundTransparency = circleObj.Filled and transparency or 1
				uiStroke.Transparency = transparency
			elseif index == "Color" then
				circleFrame.BackgroundColor3 = value
				uiStroke.Color = value
			elseif index == "Parent" then
				circleFrame.Parent = value
			end
			circleObj[index] = value
		end,
		__index = function(self, index)
			if index == "Remove" or index == "Destroy" then
				return function()
					circleFrame:Destroy()
					circleObj:Remove()
				end
			end
			return circleObj[index]
		end,
		__tostring = function() return "Drawing" end
	})
end

)__DRAWINGUA__"
R"__DRAWINGUA__(function DrawingLib.createSquare()
	local squareObj = ({
		Size = Vector2.zero,
		Position = Vector2.zero,
		Thickness = 0.7,
		Filled = false
	} + baseDrawingObj)

	local squareFrame, uiStroke = Instance.new("Frame"), Instance.new("UIStroke")
	squareFrame.Name = drawingIndex
	squareFrame.BorderSizePixel = 0

	squareFrame.Parent, uiStroke.Parent = drawingUI, squareFrame

	return setmetatable({Parent = drawingUI}, {
		__newindex = function(_, index, value)
			if squareObj[index] == nil then 
				warn("Invalid property: " .. tostring(index))
				return 
			end

			if index == "Size" then
				squareFrame.Size = UDim2.fromOffset(value.X, value.Y)
			elseif index == "Position" then
				squareFrame.Position = UDim2.fromOffset(value.X, value.Y)
			elseif index == "Thickness" then
				uiStroke.Thickness = math.clamp(value, 0.6, 0x7fffffff)
			elseif index == "Filled" then
				squareFrame.BackgroundTransparency = value and convertTransparency(squareObj.Transparency) or 1
				uiStroke.Enabled = not value
			elseif index == "Visible" then
				squareFrame.Visible = value
			elseif index == "ZIndex" then
				squareFrame.ZIndex = value
			elseif index == "Transparency" then
				local transparency = convertTransparency(value)
				squareFrame.BackgroundTransparency = squareObj.Filled and transparency or 1
				uiStroke.Transparency = transparency
			elseif index == "Color" then
				squareFrame.BackgroundColor3 = value
				uiStroke.Color = value
			elseif index == "Parent" then
				squareFrame.Parent = value
			end
			squareObj[index] = value
		end,
		__index = function(self, index)
			if index == "Remove" or index == "Destroy" then
				return function()
					squareFrame:Destroy()
					squareObj:Remove()
				end
			end
			return squareObj[index]
		end,
		__tostring = function() return "Drawing" end
	})
end

)__DRAWINGUA__"
R"__DRAWINGUA__(function DrawingLib.createImage()
	local imageObj = ({
		Data = "",
		DataURL = "rbxassetid://0",
		Size = Vector2.zero,
		Position = Vector2.zero
	} + baseDrawingObj)

	local imageFrame = Instance.new("ImageLabel")
	imageFrame.Name = drawingIndex
	imageFrame.BorderSizePixel = 0
	imageFrame.ScaleType = Enum.ScaleType.Stretch
	imageFrame.BackgroundTransparency = 1

	imageFrame.Parent = drawingUI

	return setmetatable({Parent = drawingUI}, {
		__newindex = function(_, index, value)
			if imageObj[index] == nil then 
				warn("Invalid property: " .. tostring(index))
				return 
			end

			if index == "Data" then
			elseif index == "DataURL" then
				imageFrame.Image = value
			elseif index == "Size" then
				imageFrame.Size = UDim2.fromOffset(value.X, value.Y)
			elseif index == "Position" then
				imageFrame.Position = UDim2.fromOffset(value.X, value.Y)
			elseif index == "Visible" then
				imageFrame.Visible = value
			elseif index == "ZIndex" then
				imageFrame.ZIndex = value
			elseif index == "Transparency" then
				imageFrame.ImageTransparency = convertTransparency(value)
			elseif index == "Color" then
				imageFrame.ImageColor3 = value
			elseif index == "Parent" then
				imageFrame.Parent = value
			end
			imageObj[index] = value
		end,
		__index = function(self, index)
			if index == "Remove" or index == "Destroy" then
				return function()
					imageFrame:Destroy()
					imageObj:Remove()
				end
			elseif index == "Data" then
				return nil 
			end
			return imageObj[index]
		end,
		__tostring = function() return "Drawing" end
	})
end

)__DRAWINGUA__"
R"__DRAWINGUA__(function DrawingLib.createQuad()
	local quadObj = ({
		PointA = Vector2.zero,
		PointB = Vector2.zero,
		PointC = Vector2.zero,
		PointD = Vector2.zero,
		Thickness = 1,
		Filled = false
	} + baseDrawingObj)

	local _linePoints = {
		A = DrawingLib.createLine(),
		B = DrawingLib.createLine(),
		C = DrawingLib.createLine(),
		D = DrawingLib.createLine()
	}

	local fillFrame = Instance.new("Frame")
	fillFrame.Name = drawingIndex .. "_Fill"
	fillFrame.BorderSizePixel = 0
	fillFrame.BackgroundTransparency = quadObj.Transparency
	fillFrame.BackgroundColor3 = quadObj.Color
	fillFrame.ZIndex = quadObj.ZIndex
	fillFrame.Visible = quadObj.Visible and quadObj.Filled

	fillFrame.Parent = drawingUI

	return setmetatable({Parent = drawingUI}, {
		__newindex = function(_, index, value)
			if quadObj[index] == nil then 
				warn("Invalid property: " .. tostring(index))
				return 
			end

			if index == "PointA" then
				_linePoints.A.From = value
				_linePoints.B.To = value
			elseif index == "PointB" then
				_linePoints.B.From = value
				_linePoints.C.To = value
			elseif index == "PointC" then
				_linePoints.C.From = value
				_linePoints.D.To = value
			elseif index == "PointD" then
				_linePoints.D.From = value
				_linePoints.A.To = value
			elseif index == "Thickness" or index == "Visible" or index == "Color" or index == "ZIndex" then
				for _, linePoint in pairs(_linePoints) do
					linePoint[index] = value
				end
				if index == "Visible" then
					fillFrame.Visible = value and quadObj.Filled
				elseif index == "Color" then
					fillFrame.BackgroundColor3 = value
				elseif index == "ZIndex" then
					fillFrame.ZIndex = value
				end
			elseif index == "Filled" then
				for _, linePoint in pairs(_linePoints) do
					linePoint.Transparency = value and 1 or quadObj.Transparency
				end
				fillFrame.Visible = value
			elseif index == "Parent" then
				fillFrame.Parent = value
			end
			quadObj[index] = value
		end,
		__index = function(self, index)
			if index == "Remove" or index == "Destroy" then
				return function()
					for _, linePoint in pairs(_linePoints) do
						linePoint:Remove()
					end
					fillFrame:Destroy()
					quadObj:Remove()
				end
			end
			return quadObj[index]
		end,
		__tostring = function() return "Drawing" end
	})
end

)__DRAWINGUA__"
R"__DRAWINGUA__(function DrawingLib.createTriangle()
	local triangleObj = ({
		PointA = Vector2.zero,
		PointB = Vector2.zero,
		PointC = Vector2.zero,
		Thickness = 1,
		Filled = false
	} + baseDrawingObj)

	local _linePoints = {
		A = DrawingLib.createLine(),
		B = DrawingLib.createLine(),
		C = DrawingLib.createLine()
	}

	local fillFrame = Instance.new("Frame")
	fillFrame.Name = drawingIndex .. "_Fill"
	fillFrame.BorderSizePixel = 0
	fillFrame.BackgroundTransparency = triangleObj.Transparency
	fillFrame.BackgroundColor3 = triangleObj.Color
	fillFrame.ZIndex = triangleObj.ZIndex
	fillFrame.Visible = triangleObj.Visible and triangleObj.Filled

	fillFrame.Parent = drawingUI

	return setmetatable({Parent = drawingUI}, {
		__newindex = function(_, index, value)
			if triangleObj[index] == nil then 
				warn("Invalid property: " .. tostring(index))
				return 
			end

			if index == "PointA" then
				_linePoints.A.From = value
				_linePoints.B.To = value
			elseif index == "PointB" then
				_linePoints.B.From = value
				_linePoints.C.To = value
			elseif index == "PointC" then
				_linePoints.C.From = value
				_linePoints.A.To = value
			elseif index == "Thickness" or index == "Visible" or index == "Color" or index == "ZIndex" then
				for _, linePoint in pairs(_linePoints) do
					linePoint[index] = value
				end
				if index == "Visible" then
					fillFrame.Visible = value and triangleObj.Filled
				elseif index == "Color" then
					fillFrame.BackgroundColor3 = value
				elseif index == "ZIndex" then
					fillFrame.ZIndex = value
				end
			elseif index == "Filled" then
				for _, linePoint in pairs(_linePoints) do
					linePoint.Transparency = value and 1 or triangleObj.Transparency
				end
				fillFrame.Visible = value
			elseif index == "Parent" then
				fillFrame.Parent = value
			end
			triangleObj[index] = value
		end,
		__index = function(self, index)
			if index == "Remove" or index == "Destroy" then
				return function()
					for _, linePoint in pairs(_linePoints) do
						linePoint:Remove()
					end
					fillFrame:Destroy()
					triangleObj:Remove()
				end
			end
			return triangleObj[index]
		end,
		__tostring = function() return "Drawing" end
	})
end

)__DRAWINGUA__"
R"__DRAWINGUA__(function DrawingLib.createFrame()
	local frameObj = ({
		Size = UDim2.new(0, 100, 0, 100),
		Position = UDim2.new(0, 0, 0, 0),
		Color = Color3.new(1, 1, 1),
		Transparency = 0,
		Visible = true,
		ZIndex = 1
	} + baseDrawingObj)

	local frame = Instance.new("Frame")
	frame.Name = drawingIndex
	frame.Size = frameObj.Size
	frame.Position = frameObj.Position
	frame.BackgroundColor3 = frameObj.Color
	frame.BackgroundTransparency = convertTransparency(frameObj.Transparency)
	frame.Visible = frameObj.Visible
	frame.ZIndex = frameObj.ZIndex
	frame.BorderSizePixel = 0

	frame.Parent = drawingUI

	return setmetatable({Parent = drawingUI}, {
		__newindex = function(_, index, value)
			if frameObj[index] == nil then
				warn("Invalid property: " .. tostring(index))
				return
			end

			if index == "Size" then
				frame.Size = value
			elseif index == "Position" then
				frame.Position = value
			elseif index == "Color" then
				frame.BackgroundColor3 = value
			elseif index == "Transparency" then
				frame.BackgroundTransparency = convertTransparency(value)
			elseif index == "Visible" then
				frame.Visible = value
			elseif index == "ZIndex" then
				frame.ZIndex = value
			elseif index == "Parent" then
				frame.Parent = value
			end
			frameObj[index] = value
		end,
		__index = function(self, index)
			if index == "Remove" or index == "Destroy" then
				return function()
					frame:Destroy()
					frameObj:Remove()
				end
			end
			return frameObj[index]
		end,
		__tostring = function() return "Drawing" end
	})
end

)__DRAWINGUA__"
R"__DRAWINGUA__(function DrawingLib.createScreenGui()
	local screenGuiObj = ({
		IgnoreGuiInset = true,
		DisplayOrder = 0,
		ResetOnSpawn = true,
		ZIndexBehavior = Enum.ZIndexBehavior.Sibling,
		Enabled = true
	} + baseDrawingObj)

	local screenGui = Instance.new("ScreenGui")
	screenGui.Name = drawingIndex
	screenGui.IgnoreGuiInset = screenGuiObj.IgnoreGuiInset
	screenGui.DisplayOrder = screenGuiObj.DisplayOrder
	screenGui.ResetOnSpawn = screenGuiObj.ResetOnSpawn
	screenGui.ZIndexBehavior = screenGuiObj.ZIndexBehavior
	screenGui.Enabled = screenGuiObj.Enabled

	screenGui.Parent = coreGui

	return setmetatable({Parent = coreGui}, {
		__newindex = function(_, index, value)
			if screenGuiObj[index] == nil then
				warn("Invalid property: " .. tostring(index))
				return
			end

			if index == "IgnoreGuiInset" then
				screenGui.IgnoreGuiInset = value
			elseif index == "DisplayOrder" then
				screenGui.DisplayOrder = value
			elseif index == "ResetOnSpawn" then
				screenGui.ResetOnSpawn = value
			elseif index == "ZIndexBehavior" then
				screenGui.ZIndexBehavior = value
			elseif index == "Enabled" then
				screenGui.Enabled = value
			elseif index == "Parent" then
				screenGui.Parent = value
			end
			screenGuiObj[index] = value
		end,
		__index = function(self, index)
			if index == "Remove" or index == "Destroy" then
				return function()
					screenGui:Destroy()
					screenGuiObj:Remove()
				end
			end
			return screenGuiObj[index]
		end,
		__tostring = function() return "Drawing" end
	})
end

)__DRAWINGUA__"
R"__DRAWINGUA__(function DrawingLib.createTextButton()
	local buttonObj = ({
		Text = "Button",
		Font = DrawingLib.Fonts.UI,
		Size = 20,
		Position = UDim2.new(0, 0, 0, 0),
		Color = Color3.new(1, 1, 1),
		BackgroundColor = Color3.new(0.2, 0.2, 0.2),
		Transparency = 0,
		Visible = true,
		ZIndex = 1,
		MouseButton1Click = nil
	} + baseDrawingObj)

	local button = Instance.new("TextButton")
	button.Name = drawingIndex
	button.Text = buttonObj.Text
	button.FontFace = getFontFromIndex(buttonObj.Font)
	button.TextSize = buttonObj.Size
	button.Position = buttonObj.Position
	button.TextColor3 = buttonObj.Color
	button.BackgroundColor3 = buttonObj.BackgroundColor
	button.BackgroundTransparency = convertTransparency(buttonObj.Transparency)
	button.Visible = buttonObj.Visible
	button.ZIndex = buttonObj.ZIndex

	button.Parent = drawingUI

	local buttonEvents = {}

	return setmetatable({
		Parent = drawingUI,
		Connect = function(_, eventName, callback)
			if eventName == "MouseButton1Click" then
				if buttonEvents["MouseButton1Click"] then
					buttonEvents["MouseButton1Click"]:Disconnect()
				end
				buttonEvents["MouseButton1Click"] = button.MouseButton1Click:Connect(callback)
			else
				warn("Invalid event: " .. tostring(eventName))
			end
		end
	}, {
		__newindex = function(_, index, value)
			if buttonObj[index] == nil then
				warn("Invalid property: " .. tostring(index))
				return
			end

			if index == "Text" then
				button.Text = value
			elseif index == "Font" then
				button.FontFace = getFontFromIndex(math.clamp(value, 0, 3))
			elseif index == "Size" then
				button.TextSize = value
			elseif index == "Position" then
				button.Position = value
			elseif index == "Color" then
				button.TextColor3 = value
			elseif index == "BackgroundColor" then
				button.BackgroundColor3 = value
			elseif index == "Transparency" then
				button.BackgroundTransparency = convertTransparency(value)
			elseif index == "Visible" then
				button.Visible = value
			elseif index == "ZIndex" then
				button.ZIndex = value
			elseif index == "Parent" then
				button.Parent = value
			elseif index == "MouseButton1Click" then
				if typeof(value) == "function" then
					if buttonEvents["MouseButton1Click"] then
						buttonEvents["MouseButton1Click"]:Disconnect()
					end
					buttonEvents["MouseButton1Click"] = button.MouseButton1Click:Connect(value)
				else
					warn("Invalid value for MouseButton1Click: expected function, got " .. typeof(value))
				end
			end
			buttonObj[index] = value
		end,
		__index = function(self, index)
			if index == "Remove" or index == "Destroy" then
				return function()
					button:Destroy()
					buttonObj:Remove()
				end
			end
			return buttonObj[index]
		end,
		__tostring = function() return "Drawing" end
	})
end

)__DRAWINGUA__"
R"__DRAWINGUA__(function DrawingLib.createTextLabel()
	local labelObj = ({
		Text = "Label",
		Font = DrawingLib.Fonts.UI,
		Size = 20,
		Position = UDim2.new(0, 0, 0, 0),
		Color = Color3.new(1, 1, 1),
		BackgroundColor = Color3.new(0.2, 0.2, 0.2),
		Transparency = 0,
		Visible = true,
		ZIndex = 1
	} + baseDrawingObj)

	local label = Instance.new("TextLabel")
	label.Name = drawingIndex
	label.Text = labelObj.Text
	label.FontFace = getFontFromIndex(labelObj.Font)
	label.TextSize = labelObj.Size
	label.Position = labelObj.Position
	label.TextColor3 = labelObj.Color
	label.BackgroundColor3 = labelObj.BackgroundColor
	label.BackgroundTransparency = convertTransparency(labelObj.Transparency)
	label.Visible = labelObj.Visible
	label.ZIndex = labelObj.ZIndex

	label.Parent = drawingUI

	return setmetatable({Parent = drawingUI}, {
		__newindex = function(_, index, value)
			if labelObj[index] == nil then
				warn("Invalid property: " .. tostring(index))
				return
			end

			if index == "Text" then
				label.Text = value
			elseif index == "Font" then
				label.FontFace = getFontFromIndex(math.clamp(value, 0, 3))
			elseif index == "Size" then
				label.TextSize = value
			elseif index == "Position" then
				label.Position = value
			elseif index == "Color" then
				label.TextColor3 = value
			elseif index == "BackgroundColor" then
				label.BackgroundColor3 = value
			elseif index == "Transparency" then
				label.BackgroundTransparency = convertTransparency(value)
			elseif index == "Visible" then
				label.Visible = value
			elseif index == "ZIndex" then
				label.ZIndex = value
			elseif index == "Parent" then
				label.Parent = value
			end
			labelObj[index] = value
		end,
		__index = function(self, index)
			if index == "Remove" or index == "Destroy" then
				return function()
					label:Destroy()
					labelObj:Remove()
				end
			end
			return labelObj[index]
		end,
		__tostring = function() return "Drawing" end
	})
end

)__DRAWINGUA__"
R"__DRAWINGUA__(function DrawingLib.createTextBox()
	local boxObj = ({
		Text = "",
		Font = DrawingLib.Fonts.UI,
		Size = 20,
		Position = UDim2.new(0, 0, 0, 0),
		Color = Color3.new(1, 1, 1),
		BackgroundColor = Color3.new(0.2, 0.2, 0.2),
		Transparency = 0,
		Visible = true,
		ZIndex = 1
	} + baseDrawingObj)

	local textBox = Instance.new("TextBox")
	textBox.Name = drawingIndex
	textBox.Text = boxObj.Text
	textBox.FontFace = getFontFromIndex(boxObj.Font)
	textBox.TextSize = boxObj.Size
	textBox.Position = boxObj.Position
	textBox.TextColor3 = boxObj.Color
	textBox.BackgroundColor3 = boxObj.BackgroundColor
	textBox.BackgroundTransparency = convertTransparency(boxObj.Transparency)
	textBox.Visible = boxObj.Visible
	textBox.ZIndex = boxObj.ZIndex

	textBox.Parent = drawingUI

	return setmetatable({Parent = drawingUI}, {
		__newindex = function(_, index, value)
			if boxObj[index] == nil then
				warn("Invalid property: " .. tostring(index))
				return
			end

			if index == "Text" then
				textBox.Text = value
			elseif index == "Font" then
				textBox.FontFace = getFontFromIndex(math.clamp(value, 0, 3))
			elseif index == "Size" then
				textBox.TextSize = value
			elseif index == "Position" then
				textBox.Position = value
			elseif index == "Color" then
				textBox.TextColor3 = value
			elseif index == "BackgroundColor" then
				textBox.BackgroundColor3 = value
			elseif index == "Transparency" then
				textBox.BackgroundTransparency = convertTransparency(value)
			elseif index == "Visible" then
				textBox.Visible = value
			elseif index == "ZIndex" then
				textBox.ZIndex = value
			elseif index == "Parent" then
				textBox.Parent = value
			end
			boxObj[index] = value
		end,
		__index = function(self, index)
			if index == "Remove" or index == "Destroy" then
				return function()
					textBox:Destroy()
					boxObj:Remove()
				end
			end
			return boxObj[index]
		end,
		__tostring = function() return "Drawing" end
	})
end

)__DRAWINGUA__"
R"__DRAWINGUA__(local drawingFunctions = {}

function drawingFunctions.isrenderobj(drawingObj)
	local success, isrenderobj = pcall(function()
		return drawingObj.Parent == drawingUI
	end)
	if not success then return false end
	return isrenderobj
end

function drawingFunctions.getrenderproperty(drawingObj, property)
	local success, drawingProperty  = pcall(function()
		return drawingObj[property]
	end)
	if not success then return end

	if drawingProperty ~= nil then
		return drawingProperty
	end
end

function drawingFunctions.setrenderproperty(drawingObj, property, value)
	assert(drawingFunctions.getrenderproperty(drawingObj, property), "'" .. tostring(property) .. "' is not a valid property of " .. tostring(drawingObj) .. ", " .. tostring(typeof(drawingObj)))
	drawingObj[property]  = value
end

function drawingFunctions.cleardrawcache()
	for _, drawing in drawingUI:GetDescendants() do
		drawing:Remove()
	end
end

return {Drawing = DrawingLib, functions = drawingFunctions})__DRAWINGUA__";

const std::string g_HashLibUA =
    std::string(R"__HASHUA__(
local Alphabet = {}
local Indexes = {}

-- A-Z
for Index = 65, 90 do
	table.insert(Alphabet, Index)
end

-- a-z
for Index = 97, 122 do
	table.insert(Alphabet, Index)
end

-- 0-9
for Index = 48, 57 do
	table.insert(Alphabet, Index)
end

table.insert(Alphabet, 43) -- +
table.insert(Alphabet, 47) -- /

for Index, Character in ipairs(Alphabet) do
	Indexes[Character] = Index
end

local Base64 = {}

local bit32_rshift = bit32.rshift
local bit32_lshift = bit32.lshift
local bit32_band = bit32.band

--[[**
	Encodes a string in Base64.
	@param [t:string] Input The input string to encode.
	@returns [t:string] The string encoded in Base64.
**--]]
function Base64.Encode(Input)
	local Output = {}
	local Length = 0

	for Index = 1, #Input, 3 do
		local C1, C2, C3 = string.byte(Input, Index, Index + 2)

		local A = bit32_rshift(C1, 2)
		local B = bit32_lshift(bit32_band(C1, 3), 4) + bit32_rshift(C2 or 0, 4)
		local C = bit32_lshift(bit32_band(C2 or 0, 15), 2) + bit32_rshift(C3 or 0, 6)
		local D = bit32_band(C3 or 0, 63)

		Length = Length + 1
		Output[Length] = Alphabet[A + 1]

		Length = Length + 1
		Output[Length] = Alphabet[B + 1]

		Length = Length + 1
		Output[Length] = C2 and Alphabet[C + 1] or 61

		Length = Length + 1
		Output[Length] = C3 and Alphabet[D + 1] or 61
	end

	local NewOutput = {}
	local NewLength = 0
	local IndexAdd4096Sub1

	for Index = 1, Length, 4096 do
		NewLength = NewLength + 1
		IndexAdd4096Sub1 = Index + 4096 - 1

		NewOutput[NewLength] = string.char(table.unpack(
			Output,
			Index,
			IndexAdd4096Sub1 > Length and Length or IndexAdd4096Sub1
			))
	end

	return table.concat(NewOutput)
end

--[[**
	Decodes a string from Base64.
	@param [t:string] Input The input string to decode.
	@returns [t:string] The newly decoded string.
**--]]
function Base64.Decode(Input)
	local Output = {}
	local Length = 0

	for Index = 1, #Input, 4 do
		local C1, C2, C3, C4 = string.byte(Input, Index, Index + 3)

		local I1 = Indexes[C1] - 1
		local I2 = Indexes[C2] - 1
		local I3 = (Indexes[C3] or 1) - 1
		local I4 = (Indexes[C4] or 1) - 1

		local A = bit32_lshift(I1, 2) + bit32_rshift(I2, 4)
		local B = bit32_lshift(bit32_band(I2, 15), 4) + bit32_rshift(I3, 2)
		local C = bit32_lshift(bit32_band(I3, 3), 6) + I4

		Length = Length + 1
		Output[Length] = A

		if C3 ~= 61 then
			Length = Length + 1
			Output[Length] = B
		end

		if C4 ~= 61 then
			Length = Length + 1
			Output[Length] = C
		end
	end

	local NewOutput = {}
	local NewLength = 0
	local IndexAdd4096Sub1

	for Index = 1, Length, 4096 do
		NewLength = NewLength + 1
		IndexAdd4096Sub1 = Index + 4096 - 1

		NewOutput[NewLength] = string.char(table.unpack(
			Output,
			Index,
			IndexAdd4096Sub1 > Length and Length or IndexAdd4096Sub1
			))
	end

	return table.concat(NewOutput)
end

--------------------------------------------------------------------------------
-- LOCALIZATION FOR VM OPTIMIZATIONS
--------------------------------------------------------------------------------

local ipairs = ipairs

--------------------------------------------------------------------------------
-- 32-BIT BITWISE FUNCTIONS
--------------------------------------------------------------------------------
-- Only low 32 bits of function arguments matter, high bits are ignored
-- The result of all functions (except HEX) is an integer inside "correct range":
-- for "bit" library:    (-TWO_POW_31)..(TWO_POW_31-1)
-- for "bit32" library:        0..(TWO_POW_32-1)
local bit32_band = bit32.band -- 2 arguments
local bit32_bor = bit32.bor -- 2 arguments
local bit32_bxor = bit32.bxor -- 2..5 arguments
local bit32_lshift = bit32.lshift -- second argument is integer 0..31
local bit32_rshift = bit32.rshift -- second argument is integer 0..31
local bit32_lrotate = bit32.lrotate -- second argument is integer 0..31
local bit32_rrotate = bit32.rrotate -- second argument is integer 0..31

--------------------------------------------------------------------------------
-- CREATING OPTIMIZED INNER LOOP
--------------------------------------------------------------------------------
-- Arrays of SHA2 "magic numbers" (in "INT64" and "FFI" branches "*_lo" arrays contain 64-bit values)
local sha2_K_lo, sha2_K_hi, sha2_H_lo, sha2_H_hi, sha3_RC_lo, sha3_RC_hi = {}, {}, {}, {}, {}, {}
local sha2_H_ext256 = {
	[224] = {};
	[256] = sha2_H_hi;
}

local sha2_H_ext512_lo, sha2_H_ext512_hi = {
	[384] = {};
	[512] = sha2_H_lo;
}, {
	[384] = {};
	[512] = sha2_H_hi;
}

local md5_K, md5_sha1_H = {}, {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0}
local md5_next_shift = {0, 0, 0, 0, 0, 0, 0, 0, 28, 25, 26, 27, 0, 0, 10, 9, 11, 12, 0, 15, 16, 17, 18, 0, 20, 22, 23, 21}
local HEX64, XOR64A5, lanes_index_base -- defined only for branches that internally use 64-bit integers: "INT64" and "FFI"
local common_W = {} -- temporary table shared between all calculations (to avoid creating new temporary table every time)
local K_lo_modulo, hi_factor, hi_factor_keccak = 4294967296, 0, 0

local TWO_POW_NEG_56 = 2 ^ -56
local TWO_POW_NEG_17 = 2 ^ -17

local TWO_POW_2 = 2 ^ 2
local TWO_POW_3 = 2 ^ 3
local TWO_POW_4 = 2 ^ 4
local TWO_POW_5 = 2 ^ 5
local TWO_POW_6 = 2 ^ 6
local TWO_POW_7 = 2 ^ 7
local TWO_POW_8 = 2 ^ 8
local TWO_POW_9 = 2 ^ 9
local TWO_POW_10 = 2 ^ 10
local TWO_POW_11 = 2 ^ 11
local TWO_POW_12 = 2 ^ 12
local TWO_POW_13 = 2 ^ 13
local TWO_POW_14 = 2 ^ 14
local TWO_POW_15 = 2 ^ 15
local TWO_POW_16 = 2 ^ 16
local TWO_POW_17 = 2 ^ 17
local TWO_POW_18 = 2 ^ 18
local TWO_POW_19 = 2 ^ 19
local TWO_POW_20 = 2 ^ 20
local TWO_POW_21 = 2 ^ 21
local TWO_POW_22 = 2 ^ 22
local TWO_POW_23 = 2 ^ 23
local TWO_POW_24 = 2 ^ 24
local TWO_POW_25 = 2 ^ 25
local TWO_POW_26 = 2 ^ 26
local TWO_POW_27 = 2 ^ 27
local TWO_POW_28 = 2 ^ 28
local TWO_POW_29 = 2 ^ 29
local TWO_POW_30 = 2 ^ 30
local TWO_POW_31 = 2 ^ 31
local TWO_POW_32 = 2 ^ 32
local TWO_POW_40 = 2 ^ 40

local TWO56_POW_7 = 256 ^ 7

-- Implementation for Lua 5.1/5.2 (with or without bitwise library available)
local function sha256_feed_64(H, str, offs, size)
	-- offs >= 0, size >= 0, size is multiple of 64
	local W, K = common_W, sha2_K_hi
	local h1, h2, h3, h4, h5, h6, h7, h8 = H[1], H[2], H[3], H[4], H[5], H[6], H[7], H[8]
	for pos = offs, offs + size - 1, 64 do
		for j = 1, 16 do
			pos = pos + 4
			local a, b, c, d = string.byte(str, pos - 3, pos)
			W[j] = ((a * 256 + b) * 256 + c) * 256 + d
		end

		for j = 17, 64 do
			local a, b = W[j - 15], W[j - 2]
			W[j] = bit32_bxor(bit32_rrotate(a, 7), bit32_lrotate(a, 14), bit32_rshift(a, 3)) + bit32_bxor(bit32_lrotate(b, 15), bit32_lrotate(b, 13), bit32_rshift(b, 10)) + W[j - 7] + W[j - 16]
		end

		local a, b, c, d, e, f, g, h = h1, h2, h3, h4, h5, h6, h7, h8
		for j = 1, 64 do
			local z = bit32_bxor(bit32_rrotate(e, 6), bit32_rrotate(e, 11), bit32_lrotate(e, 7)) + bit32_band(e, f) + bit32_band(-1 - e, g) + h + K[j] + W[j]
			h = g
			g = f
			f = e
			e = z + d
			d = c
			c = b
			b = a
			a = z + bit32_band(d, c) + bit32_band(a, bit32_bxor(d, c)) + bit32_bxor(bit32_rrotate(a, 2), bit32_rrotate(a, 13), bit32_lrotate(a, 10))
		end

		h1, h2, h3, h4 = (a + h1) % 4294967296, (b + h2) % 4294967296, (c + h3) % 4294967296, (d + h4) % 4294967296
		h5, h6, h7, h8 = (e + h5) % 4294967296, (f + h6) % 4294967296, (g + h7) % 4294967296, (h + h8) % 4294967296
	end

	H[1], H[2], H[3], H[4], H[5], H[6], H[7], H[8] = h1, h2, h3, h4, h5, h6, h7, h8
end

local function sha512_feed_128(H_lo, H_hi, str, offs, size)
	-- offs >= 0, size >= 0, size is multiple of 128
	-- W1_hi, W1_lo, W2_hi, W2_lo, ...   Wk_hi = W[2*k-1], Wk_lo = W[2*k]
	local W, K_lo, K_hi = common_W, sha2_K_lo, sha2_K_hi
	local h1_lo, h2_lo, h3_lo, h4_lo, h5_lo, h6_lo, h7_lo, h8_lo = H_lo[1], H_lo[2], H_lo[3], H_lo[4], H_lo[5], H_lo[6], H_lo[7], H_lo[8]
	local h1_hi, h2_hi, h3_hi, h4_hi, h5_hi, h6_hi, h7_hi, h8_hi = H_hi[1], H_hi[2], H_hi[3], H_hi[4], H_hi[5], H_hi[6], H_hi[7], H_hi[8]
	for pos = offs, offs + size - 1, 128 do
		for j = 1, 16 * 2 do
			pos = pos + 4
			local a, b, c, d = string.byte(str, pos - 3, pos)
			W[j] = ((a * 256 + b) * 256 + c) * 256 + d
		end

		for jj = 34, 160, 2 do
			local a_lo, a_hi, b_lo, b_hi = W[jj - 30], W[jj - 31], W[jj - 4], W[jj - 5]
			local tmp1 = bit32_bxor(bit32_rshift(a_lo, 1) + bit32_lshift(a_hi, 31), bit32_rshift(a_lo, 8) + bit32_lshift(a_hi, 24), bit32_rshift(a_lo, 7) + bit32_lshift(a_hi, 25)) % 4294967296 +
				bit32_bxor(bit32_rshift(b_lo, 19) + bit32_lshift(b_hi, 13), bit32_lshift(b_lo, 3) + bit32_rshift(b_hi, 29), bit32_rshift(b_lo, 6) + bit32_lshift(b_hi, 26)) % 4294967296 +
				W[jj - 14] + W[jj - 32]

			local tmp2 = tmp1 % 4294967296
			W[jj - 1] = bit32_bxor(bit32_rshift(a_hi, 1) + bit32_lshift(a_lo, 31), bit32_rshift(a_hi, 8) + bit32_lshift(a_lo, 24), bit32_rshift(a_hi, 7)) +
				bit32_bxor(bit32_rshift(b_hi, 19) + bit32_lshift(b_lo, 13), bit32_lshift(b_hi, 3) + bit32_rshift(b_lo, 29), bit32_rshift(b_hi, 6)) +
				W[jj - 15] + W[jj - 33] + (tmp1 - tmp2) / 4294967296

			W[jj] = tmp2
		end

		local a_lo, b_lo, c_lo, d_lo, e_lo, f_lo, g_lo, h_lo = h1_lo, h2_lo, h3_lo, h4_lo, h5_lo, h6_lo, h7_lo, h8_lo
		local a_hi, b_hi, c_hi, d_hi, e_hi, f_hi, g_hi, h_hi = h1_hi, h2_hi, h3_hi, h4_hi, h5_hi, h6_hi, h7_hi, h8_hi
		for j = 1, 80 do
			local jj = 2 * j
			local tmp1 = bit32_bxor(bit32_rshift(e_lo, 14) + bit32_lshift(e_hi, 18), bit32_rshift(e_lo, 18) + bit32_lshift(e_hi, 14), bit32_lshift(e_lo, 23) + bit32_rshift(e_hi, 9)) % 4294967296 +
				(bit32_band(e_lo, f_lo) + bit32_band(-1 - e_lo, g_lo)) % 4294967296 +
				h_lo + K_lo[j] + W[jj]

			local z_lo = tmp1 % 4294967296
			local z_hi = bit32_bxor(bit32_rshift(e_hi, 14) + bit32_lshift(e_lo, 18), bit32_rshift(e_hi, 18) + bit32_lshift(e_lo, 14), bit32_lshift(e_hi, 23) + bit32_rshift(e_lo, 9)) +
				bit32_band(e_hi, f_hi) + bit32_band(-1 - e_hi, g_hi) +
				h_hi + K_hi[j] + W[jj - 1] +
				(tmp1 - z_lo) / 4294967296

			h_lo = g_lo
			h_hi = g_hi
			g_lo = f_lo
			g_hi = f_hi
			f_lo = e_lo
			f_hi = e_hi
			tmp1 = z_lo + d_lo
			e_lo = tmp1 % 4294967296
			e_hi = z_hi + d_hi + (tmp1 - e_lo) / 4294967296
			d_lo = c_lo
			d_hi = c_hi
			c_lo = b_lo
			c_hi = b_hi
			b_lo = a_lo
			b_hi = a_hi
			tmp1 = z_lo + (bit32_band(d_lo, c_lo) + bit32_band(b_lo, bit32_bxor(d_lo, c_lo))) % 4294967296 + bit32_bxor(bit32_rshift(b_lo, 28) + bit32_lshift(b_hi, 4), bit32_lshift(b_lo, 30) + bit32_rshift(b_hi, 2), bit32_lshift(b_lo, 25) + bit32_rshift(b_hi, 7)) % 4294967296
			a_lo = tmp1 % 4294967296
			a_hi = z_hi + (bit32_band(d_hi, c_hi) + bit32_band(b_hi, bit32_bxor(d_hi, c_hi))) + bit32_bxor(bit32_rshift(b_hi, 28) + bit32_lshift(b_lo, 4), bit32_lshift(b_hi, 30) + bit32_rshift(b_lo, 2), bit32_lshift(b_hi, 25) + bit32_rshift(b_lo, 7)) + (tmp1 - a_lo) / 4294967296
		end

		a_lo = h1_lo + a_lo
		h1_lo = a_lo % 4294967296
		h1_hi = (h1_hi + a_hi + (a_lo - h1_lo) / 4294967296) % 4294967296
		a_lo = h2_lo + b_lo
		h2_lo = a_lo % 4294967296
		h2_hi = (h2_hi + b_hi + (a_lo - h2_lo) / 4294967296) % 4294967296
		a_lo = h3_lo + c_lo
		h3_lo = a_lo % 4294967296
		h3_hi = (h3_hi + c_hi + (a_lo - h3_lo) / 4294967296) % 4294967296
		a_lo = h4_lo + d_lo
		h4_lo = a_lo % 4294967296
		h4_hi = (h4_hi + d_hi + (a_lo - h4_lo) / 4294967296) % 4294967296
		a_lo = h5_lo + e_lo
		h5_lo = a_lo % 4294967296
		h5_hi = (h5_hi + e_hi + (a_lo - h5_lo) / 4294967296) % 4294967296
		a_lo = h6_lo + f_lo
		h6_lo = a_lo % 4294967296
		h6_hi = (h6_hi + f_hi + (a_lo - h6_lo) / 4294967296) % 4294967296
		a_lo = h7_lo + g_lo
		h7_lo = a_lo % 4294967296
		h7_hi = (h7_hi + g_hi + (a_lo - h7_lo) / 4294967296) % 4294967296
		a_lo = h8_lo + h_lo
		h8_lo = a_lo % 4294967296
		h8_hi = (h8_hi + h_hi + (a_lo - h8_lo) / 4294967296) % 4294967296
	end

	H_lo[1], H_lo[2], H_lo[3], H_lo[4], H_lo[5], H_lo[6], H_lo[7], H_lo[8] = h1_lo, h2_lo, h3_lo, h4_lo, h5_lo, h6_lo, h7_lo, h8_lo
	H_hi[1], H_hi[2], H_hi[3], H_hi[4], H_hi[5], H_hi[6], H_hi[7], H_hi[8] = h1_hi, h2_hi, h3_hi, h4_hi, h5_hi, h6_hi, h7_hi, h8_hi
end

local function md5_feed_64(H, str, offs, size)
	-- offs >= 0, size >= 0, size is multiple of 64
	local W, K, md5_next_shift = common_W, md5_K, md5_next_shift
	local h1, h2, h3, h4 = H[1], H[2], H[3], H[4]
	for pos = offs, offs + size - 1, 64 do
		for j = 1, 16 do
			pos = pos + 4
			local a, b, c, d = string.byte(str, pos - 3, pos)
			W[j] = ((d * 256 + c) * 256 + b) * 256 + a
		end

		local a, b, c, d = h1, h2, h3, h4
		local s = 25
		for j = 1, 16 do
			local F = bit32_rrotate(bit32_band(b, c) + bit32_band(-1 - b, d) + a + K[j] + W[j], s) + b
			s = md5_next_shift[s]
			a = d
			d = c
			c = b
			b = F
		end

		s = 27
		for j = 17, 32 do
			local F = bit32_rrotate(bit32_band(d, b) + bit32_band(-1 - d, c) + a + K[j] + W[(5 * j - 4) % 16 + 1], s) + b
			s = md5_next_shift[s]
			a = d
			d = c
			c = b
			b = F
		end

		s = 28
		for j = 33, 48 do
			local F = bit32_rrotate(bit32_bxor(bit32_bxor(b, c), d) + a + K[j] + W[(3 * j + 2) % 16 + 1], s) + b
			s = md5_next_shift[s]
			a = d
			d = c
			c = b
			b = F
		end

		s = 26
		for j = 49, 64 do
			local F = bit32_rrotate(bit32_bxor(c, bit32_bor(b, -1 - d)) + a + K[j] + W[(j * 7 - 7) % 16 + 1], s) + b
			s = md5_next_shift[s]
			a = d
			d = c
			c = b
			b = F
		end

		h1 = (a + h1) % 4294967296
		h2 = (b + h2) % 4294967296
		h3 = (c + h3) % 4294967296
		h4 = (d + h4) % 4294967296
	end

	H[1], H[2], H[3], H[4] = h1, h2, h3, h4
end

local function sha1_feed_64(H, str, offs, size)
	-- offs >= 0, size >= 0, size is multiple of 64
	local W = common_W
	local h1, h2, h3, h4, h5 = H[1], H[2], H[3], H[4], H[5]
	for pos = offs, offs + size - 1, 64 do
		for j = 1, 16 do
			pos = pos + 4
			local a, b, c, d = string.byte(str, pos - 3, pos)
			W[j] = ((a * 256 + b) * 256 + c) * 256 + d
		end

		for j = 17, 80 do
			W[j] = bit32_lrotate(bit32_bxor(W[j - 3], W[j - 8], W[j - 14], W[j - 16]), 1)
		end

		local a, b, c, d, e = h1, h2, h3, h4, h5
		for j = 1, 20 do
			local z = bit32_lrotate(a, 5) + bit32_band(b, c) + bit32_band(-1 - b, d) + 0x5A827999 + W[j] + e -- constant = math.floor(TWO_POW_30 * sqrt(2))
			e = d
			d = c
			c = bit32_rrotate(b, 2)
			b = a
			a = z
		end

		for j = 21, 40 do
			local z = bit32_lrotate(a, 5) + bit32_bxor(b, c, d) + 0x6ED9EBA1 + W[j] + e -- TWO_POW_30 * sqrt(3)
			e = d
			d = c
			c = bit32_rrotate(b, 2)
			b = a
			a = z
		end

		for j = 41, 60 do
			local z = bit32_lrotate(a, 5) + bit32_band(d, c) + bit32_band(b, bit32_bxor(d, c)) + 0x8F1BBCDC + W[j] + e -- TWO_POW_30 * sqrt(5)
			e = d
			d = c
			c = bit32_rrotate(b, 2)
			b = a
			a = z
		end

		for j = 61, 80 do
			local z = bit32_lrotate(a, 5) + bit32_bxor(b, c, d) + 0xCA62C1D6 + W[j] + e -- TWO_POW_30 * sqrt(10)
			e = d
			d = c
			c = bit32_rrotate(b, 2)
			b = a
			a = z
		end

		h1 = (a + h1) % 4294967296
		h2 = (b + h2) % 4294967296
		h3 = (c + h3) % 4294967296
		h4 = (d + h4) % 4294967296
		h5 = (e + h5) % 4294967296
	end

	H[1], H[2], H[3], H[4], H[5] = h1, h2, h3, h4, h5
end

local function keccak_feed(lanes_lo, lanes_hi, str, offs, size, block_size_in_bytes)
	-- This is an example of a Lua function having 79 local variables :-)
	-- offs >= 0, size >= 0, size is multiple of block_size_in_bytes, block_size_in_bytes is positive multiple of 8
	local RC_lo, RC_hi = sha3_RC_lo, sha3_RC_hi
	local qwords_qty = block_size_in_bytes / 8
	for pos = offs, offs + size - 1, block_size_in_bytes do
		for j = 1, qwords_qty do
			local a, b, c, d = string.byte(str, pos + 1, pos + 4)
			lanes_lo[j] = bit32_bxor(lanes_lo[j], ((d * 256 + c) * 256 + b) * 256 + a)
			pos = pos + 8
			a, b, c, d = string.byte(str, pos - 3, pos)
			lanes_hi[j] = bit32_bxor(lanes_hi[j], ((d * 256 + c) * 256 + b) * 256 + a)
		end

)__HASHUA__") +
R"__HASHUA__(
		local L01_lo, L01_hi, L02_lo, L02_hi, L03_lo, L03_hi, L04_lo, L04_hi, L05_lo, L05_hi, L06_lo, L06_hi, L07_lo, L07_hi, L08_lo, L08_hi, L09_lo, L09_hi, L10_lo, L10_hi, L11_lo, L11_hi, L12_lo, L12_hi, L13_lo, L13_hi, L14_lo, L14_hi, L15_lo, L15_hi, L16_lo, L16_hi, L17_lo, L17_hi, L18_lo, L18_hi, L19_lo, L19_hi, L20_lo, L20_hi, L21_lo, L21_hi, L22_lo, L22_hi, L23_lo, L23_hi, L24_lo, L24_hi, L25_lo, L25_hi = lanes_lo[1], lanes_hi[1], lanes_lo[2], lanes_hi[2], lanes_lo[3], lanes_hi[3], lanes_lo[4], lanes_hi[4], lanes_lo[5], lanes_hi[5], lanes_lo[6], lanes_hi[6], lanes_lo[7], lanes_hi[7], lanes_lo[8], lanes_hi[8], lanes_lo[9], lanes_hi[9], lanes_lo[10], lanes_hi[10], lanes_lo[11], lanes_hi[11], lanes_lo[12], lanes_hi[12], lanes_lo[13], lanes_hi[13], lanes_lo[14], lanes_hi[14], lanes_lo[15], lanes_hi[15], lanes_lo[16], lanes_hi[16], lanes_lo[17], lanes_hi[17], lanes_lo[18], lanes_hi[18], lanes_lo[19], lanes_hi[19], lanes_lo[20], lanes_hi[20], lanes_lo[21], lanes_hi[21], lanes_lo[22], lanes_hi[22], lanes_lo[23], lanes_hi[23], lanes_lo[24], lanes_hi[24], lanes_lo[25], lanes_hi[25]

		for round_idx = 1, 24 do
			local C1_lo = bit32_bxor(L01_lo, L06_lo, L11_lo, L16_lo, L21_lo)
			local C1_hi = bit32_bxor(L01_hi, L06_hi, L11_hi, L16_hi, L21_hi)
			local C2_lo = bit32_bxor(L02_lo, L07_lo, L12_lo, L17_lo, L22_lo)
			local C2_hi = bit32_bxor(L02_hi, L07_hi, L12_hi, L17_hi, L22_hi)
			local C3_lo = bit32_bxor(L03_lo, L08_lo, L13_lo, L18_lo, L23_lo)
			local C3_hi = bit32_bxor(L03_hi, L08_hi, L13_hi, L18_hi, L23_hi)
			local C4_lo = bit32_bxor(L04_lo, L09_lo, L14_lo, L19_lo, L24_lo)
			local C4_hi = bit32_bxor(L04_hi, L09_hi, L14_hi, L19_hi, L24_hi)
			local C5_lo = bit32_bxor(L05_lo, L10_lo, L15_lo, L20_lo, L25_lo)
			local C5_hi = bit32_bxor(L05_hi, L10_hi, L15_hi, L20_hi, L25_hi)

			local D_lo = bit32_bxor(C1_lo, C3_lo * 2 + (C3_hi % TWO_POW_32 - C3_hi % TWO_POW_31) / TWO_POW_31)
			local D_hi = bit32_bxor(C1_hi, C3_hi * 2 + (C3_lo % TWO_POW_32 - C3_lo % TWO_POW_31) / TWO_POW_31)

			local T0_lo = bit32_bxor(D_lo, L02_lo)
			local T0_hi = bit32_bxor(D_hi, L02_hi)
			local T1_lo = bit32_bxor(D_lo, L07_lo)
			local T1_hi = bit32_bxor(D_hi, L07_hi)
			local T2_lo = bit32_bxor(D_lo, L12_lo)
			local T2_hi = bit32_bxor(D_hi, L12_hi)
			local T3_lo = bit32_bxor(D_lo, L17_lo)
			local T3_hi = bit32_bxor(D_hi, L17_hi)
			local T4_lo = bit32_bxor(D_lo, L22_lo)
			local T4_hi = bit32_bxor(D_hi, L22_hi)

			L02_lo = (T1_lo % TWO_POW_32 - T1_lo % TWO_POW_20) / TWO_POW_20 + T1_hi * TWO_POW_12
			L02_hi = (T1_hi % TWO_POW_32 - T1_hi % TWO_POW_20) / TWO_POW_20 + T1_lo * TWO_POW_12
			L07_lo = (T3_lo % TWO_POW_32 - T3_lo % TWO_POW_19) / TWO_POW_19 + T3_hi * TWO_POW_13
			L07_hi = (T3_hi % TWO_POW_32 - T3_hi % TWO_POW_19) / TWO_POW_19 + T3_lo * TWO_POW_13
			L12_lo = T0_lo * 2 + (T0_hi % TWO_POW_32 - T0_hi % TWO_POW_31) / TWO_POW_31
			L12_hi = T0_hi * 2 + (T0_lo % TWO_POW_32 - T0_lo % TWO_POW_31) / TWO_POW_31
			L17_lo = T2_lo * TWO_POW_10 + (T2_hi % TWO_POW_32 - T2_hi % TWO_POW_22) / TWO_POW_22
			L17_hi = T2_hi * TWO_POW_10 + (T2_lo % TWO_POW_32 - T2_lo % TWO_POW_22) / TWO_POW_22
			L22_lo = T4_lo * TWO_POW_2 + (T4_hi % TWO_POW_32 - T4_hi % TWO_POW_30) / TWO_POW_30
			L22_hi = T4_hi * TWO_POW_2 + (T4_lo % TWO_POW_32 - T4_lo % TWO_POW_30) / TWO_POW_30

			D_lo = bit32_bxor(C2_lo, C4_lo * 2 + (C4_hi % TWO_POW_32 - C4_hi % TWO_POW_31) / TWO_POW_31)
			D_hi = bit32_bxor(C2_hi, C4_hi * 2 + (C4_lo % TWO_POW_32 - C4_lo % TWO_POW_31) / TWO_POW_31)

			T0_lo = bit32_bxor(D_lo, L03_lo)
			T0_hi = bit32_bxor(D_hi, L03_hi)
			T1_lo = bit32_bxor(D_lo, L08_lo)
			T1_hi = bit32_bxor(D_hi, L08_hi)
			T2_lo = bit32_bxor(D_lo, L13_lo)
			T2_hi = bit32_bxor(D_hi, L13_hi)
			T3_lo = bit32_bxor(D_lo, L18_lo)
			T3_hi = bit32_bxor(D_hi, L18_hi)
			T4_lo = bit32_bxor(D_lo, L23_lo)
			T4_hi = bit32_bxor(D_hi, L23_hi)

			L03_lo = (T2_lo % TWO_POW_32 - T2_lo % TWO_POW_21) / TWO_POW_21 + T2_hi * TWO_POW_11
			L03_hi = (T2_hi % TWO_POW_32 - T2_hi % TWO_POW_21) / TWO_POW_21 + T2_lo * TWO_POW_11
			L08_lo = (T4_lo % TWO_POW_32 - T4_lo % TWO_POW_3) / TWO_POW_3 + T4_hi * TWO_POW_29 % TWO_POW_32
			L08_hi = (T4_hi % TWO_POW_32 - T4_hi % TWO_POW_3) / TWO_POW_3 + T4_lo * TWO_POW_29 % TWO_POW_32
			L13_lo = T1_lo * TWO_POW_6 + (T1_hi % TWO_POW_32 - T1_hi % TWO_POW_26) / TWO_POW_26
			L13_hi = T1_hi * TWO_POW_6 + (T1_lo % TWO_POW_32 - T1_lo % TWO_POW_26) / TWO_POW_26
			L18_lo = T3_lo * TWO_POW_15 + (T3_hi % TWO_POW_32 - T3_hi % TWO_POW_17) / TWO_POW_17
			L18_hi = T3_hi * TWO_POW_15 + (T3_lo % TWO_POW_32 - T3_lo % TWO_POW_17) / TWO_POW_17
			L23_lo = (T0_lo % TWO_POW_32 - T0_lo % TWO_POW_2) / TWO_POW_2 + T0_hi * TWO_POW_30 % TWO_POW_32
			L23_hi = (T0_hi % TWO_POW_32 - T0_hi % TWO_POW_2) / TWO_POW_2 + T0_lo * TWO_POW_30 % TWO_POW_32

			D_lo = bit32_bxor(C3_lo, C5_lo * 2 + (C5_hi % TWO_POW_32 - C5_hi % TWO_POW_31) / TWO_POW_31)
			D_hi = bit32_bxor(C3_hi, C5_hi * 2 + (C5_lo % TWO_POW_32 - C5_lo % TWO_POW_31) / TWO_POW_31)

			T0_lo = bit32_bxor(D_lo, L04_lo)
			T0_hi = bit32_bxor(D_hi, L04_hi)
			T1_lo = bit32_bxor(D_lo, L09_lo)
			T1_hi = bit32_bxor(D_hi, L09_hi)
			T2_lo = bit32_bxor(D_lo, L14_lo)
			T2_hi = bit32_bxor(D_hi, L14_hi)
			T3_lo = bit32_bxor(D_lo, L19_lo)
			T3_hi = bit32_bxor(D_hi, L19_hi)
			T4_lo = bit32_bxor(D_lo, L24_lo)
			T4_hi = bit32_bxor(D_hi, L24_hi)

			L04_lo = T3_lo * TWO_POW_21 % TWO_POW_32 + (T3_hi % TWO_POW_32 - T3_hi % TWO_POW_11) / TWO_POW_11
			L04_hi = T3_hi * TWO_POW_21 % TWO_POW_32 + (T3_lo % TWO_POW_32 - T3_lo % TWO_POW_11) / TWO_POW_11
			L09_lo = T0_lo * TWO_POW_28 % TWO_POW_32 + (T0_hi % TWO_POW_32 - T0_hi % TWO_POW_4) / TWO_POW_4
			L09_hi = T0_hi * TWO_POW_28 % TWO_POW_32 + (T0_lo % TWO_POW_32 - T0_lo % TWO_POW_4) / TWO_POW_4
			L14_lo = T2_lo * TWO_POW_25 % TWO_POW_32 + (T2_hi % TWO_POW_32 - T2_hi % TWO_POW_7) / TWO_POW_7
			L14_hi = T2_hi * TWO_POW_25 % TWO_POW_32 + (T2_lo % TWO_POW_32 - T2_lo % TWO_POW_7) / TWO_POW_7
			L19_lo = (T4_lo % TWO_POW_32 - T4_lo % TWO_POW_8) / TWO_POW_8 + T4_hi * TWO_POW_24 % TWO_POW_32
			L19_hi = (T4_hi % TWO_POW_32 - T4_hi % TWO_POW_8) / TWO_POW_8 + T4_lo * TWO_POW_24 % TWO_POW_32
			L24_lo = (T1_lo % TWO_POW_32 - T1_lo % TWO_POW_9) / TWO_POW_9 + T1_hi * TWO_POW_23 % TWO_POW_32
			L24_hi = (T1_hi % TWO_POW_32 - T1_hi % TWO_POW_9) / TWO_POW_9 + T1_lo * TWO_POW_23 % TWO_POW_32

			D_lo = bit32_bxor(C4_lo, C1_lo * 2 + (C1_hi % TWO_POW_32 - C1_hi % TWO_POW_31) / TWO_POW_31)
			D_hi = bit32_bxor(C4_hi, C1_hi * 2 + (C1_lo % TWO_POW_32 - C1_lo % TWO_POW_31) / TWO_POW_31)

			T0_lo = bit32_bxor(D_lo, L05_lo)
			T0_hi = bit32_bxor(D_hi, L05_hi)
			T1_lo = bit32_bxor(D_lo, L10_lo)
			T1_hi = bit32_bxor(D_hi, L10_hi)
			T2_lo = bit32_bxor(D_lo, L15_lo)
			T2_hi = bit32_bxor(D_hi, L15_hi)
			T3_lo = bit32_bxor(D_lo, L20_lo)
			T3_hi = bit32_bxor(D_hi, L20_hi)
			T4_lo = bit32_bxor(D_lo, L25_lo)
			T4_hi = bit32_bxor(D_hi, L25_hi)

			L05_lo = T4_lo * TWO_POW_14 + (T4_hi % TWO_POW_32 - T4_hi % TWO_POW_18) / TWO_POW_18
			L05_hi = T4_hi * TWO_POW_14 + (T4_lo % TWO_POW_32 - T4_lo % TWO_POW_18) / TWO_POW_18
			L10_lo = T1_lo * TWO_POW_20 % TWO_POW_32 + (T1_hi % TWO_POW_32 - T1_hi % TWO_POW_12) / TWO_POW_12
			L10_hi = T1_hi * TWO_POW_20 % TWO_POW_32 + (T1_lo % TWO_POW_32 - T1_lo % TWO_POW_12) / TWO_POW_12
			L15_lo = T3_lo * TWO_POW_8 + (T3_hi % TWO_POW_32 - T3_hi % TWO_POW_24) / TWO_POW_24
			L15_hi = T3_hi * TWO_POW_8 + (T3_lo % TWO_POW_32 - T3_lo % TWO_POW_24) / TWO_POW_24
			L20_lo = T0_lo * TWO_POW_27 % TWO_POW_32 + (T0_hi % TWO_POW_32 - T0_hi % TWO_POW_5) / TWO_POW_5
			L20_hi = T0_hi * TWO_POW_27 % TWO_POW_32 + (T0_lo % TWO_POW_32 - T0_lo % TWO_POW_5) / TWO_POW_5
			L25_lo = (T2_lo % TWO_POW_32 - T2_lo % TWO_POW_25) / TWO_POW_25 + T2_hi * TWO_POW_7
			L25_hi = (T2_hi % TWO_POW_32 - T2_hi % TWO_POW_25) / TWO_POW_25 + T2_lo * TWO_POW_7
)__HASHUA__" +
R"__HASHUA__(
			D_lo = bit32_bxor(C5_lo, C2_lo * 2 + (C2_hi % TWO_POW_32 - C2_hi % TWO_POW_31) / TWO_POW_31)
			D_hi = bit32_bxor(C5_hi, C2_hi * 2 + (C2_lo % TWO_POW_32 - C2_lo % TWO_POW_31) / TWO_POW_31)

			T1_lo = bit32_bxor(D_lo, L06_lo)
			T1_hi = bit32_bxor(D_hi, L06_hi)
			T2_lo = bit32_bxor(D_lo, L11_lo)
			T2_hi = bit32_bxor(D_hi, L11_hi)
			T3_lo = bit32_bxor(D_lo, L16_lo)
			T3_hi = bit32_bxor(D_hi, L16_hi)
			T4_lo = bit32_bxor(D_lo, L21_lo)
			T4_hi = bit32_bxor(D_hi, L21_hi)

			L06_lo = T2_lo * TWO_POW_3 + (T2_hi % TWO_POW_32 - T2_hi % TWO_POW_29) / TWO_POW_29
			L06_hi = T2_hi * TWO_POW_3 + (T2_lo % TWO_POW_32 - T2_lo % TWO_POW_29) / TWO_POW_29
			L11_lo = T4_lo * TWO_POW_18 + (T4_hi % TWO_POW_32 - T4_hi % TWO_POW_14) / TWO_POW_14
			L11_hi = T4_hi * TWO_POW_18 + (T4_lo % TWO_POW_32 - T4_lo % TWO_POW_14) / TWO_POW_14
			L16_lo = (T1_lo % TWO_POW_32 - T1_lo % TWO_POW_28) / TWO_POW_28 + T1_hi * TWO_POW_4
			L16_hi = (T1_hi % TWO_POW_32 - T1_hi % TWO_POW_28) / TWO_POW_28 + T1_lo * TWO_POW_4
			L21_lo = (T3_lo % TWO_POW_32 - T3_lo % TWO_POW_23) / TWO_POW_23 + T3_hi * TWO_POW_9
			L21_hi = (T3_hi % TWO_POW_32 - T3_hi % TWO_POW_23) / TWO_POW_23 + T3_lo * TWO_POW_9

			L01_lo = bit32_bxor(D_lo, L01_lo)
			L01_hi = bit32_bxor(D_hi, L01_hi)
			L01_lo, L02_lo, L03_lo, L04_lo, L05_lo = bit32_bxor(L01_lo, bit32_band(-1 - L02_lo, L03_lo)), bit32_bxor(L02_lo, bit32_band(-1 - L03_lo, L04_lo)), bit32_bxor(L03_lo, bit32_band(-1 - L04_lo, L05_lo)), bit32_bxor(L04_lo, bit32_band(-1 - L05_lo, L01_lo)), bit32_bxor(L05_lo, bit32_band(-1 - L01_lo, L02_lo))
			L01_hi, L02_hi, L03_hi, L04_hi, L05_hi = bit32_bxor(L01_hi, bit32_band(-1 - L02_hi, L03_hi)), bit32_bxor(L02_hi, bit32_band(-1 - L03_hi, L04_hi)), bit32_bxor(L03_hi, bit32_band(-1 - L04_hi, L05_hi)), bit32_bxor(L04_hi, bit32_band(-1 - L05_hi, L01_hi)), bit32_bxor(L05_hi, bit32_band(-1 - L01_hi, L02_hi))
			L06_lo, L07_lo, L08_lo, L09_lo, L10_lo = bit32_bxor(L09_lo, bit32_band(-1 - L10_lo, L06_lo)), bit32_bxor(L10_lo, bit32_band(-1 - L06_lo, L07_lo)), bit32_bxor(L06_lo, bit32_band(-1 - L07_lo, L08_lo)), bit32_bxor(L07_lo, bit32_band(-1 - L08_lo, L09_lo)), bit32_bxor(L08_lo, bit32_band(-1 - L09_lo, L10_lo))
			L06_hi, L07_hi, L08_hi, L09_hi, L10_hi = bit32_bxor(L09_hi, bit32_band(-1 - L10_hi, L06_hi)), bit32_bxor(L10_hi, bit32_band(-1 - L06_hi, L07_hi)), bit32_bxor(L06_hi, bit32_band(-1 - L07_hi, L08_hi)), bit32_bxor(L07_hi, bit32_band(-1 - L08_hi, L09_hi)), bit32_bxor(L08_hi, bit32_band(-1 - L09_hi, L10_hi))
			L11_lo, L12_lo, L13_lo, L14_lo, L15_lo = bit32_bxor(L12_lo, bit32_band(-1 - L13_lo, L14_lo)), bit32_bxor(L13_lo, bit32_band(-1 - L14_lo, L15_lo)), bit32_bxor(L14_lo, bit32_band(-1 - L15_lo, L11_lo)), bit32_bxor(L15_lo, bit32_band(-1 - L11_lo, L12_lo)), bit32_bxor(L11_lo, bit32_band(-1 - L12_lo, L13_lo))
			L11_hi, L12_hi, L13_hi, L14_hi, L15_hi = bit32_bxor(L12_hi, bit32_band(-1 - L13_hi, L14_hi)), bit32_bxor(L13_hi, bit32_band(-1 - L14_hi, L15_hi)), bit32_bxor(L14_hi, bit32_band(-1 - L15_hi, L11_hi)), bit32_bxor(L15_hi, bit32_band(-1 - L11_hi, L12_hi)), bit32_bxor(L11_hi, bit32_band(-1 - L12_hi, L13_hi))
			L16_lo, L17_lo, L18_lo, L19_lo, L20_lo = bit32_bxor(L20_lo, bit32_band(-1 - L16_lo, L17_lo)), bit32_bxor(L16_lo, bit32_band(-1 - L17_lo, L18_lo)), bit32_bxor(L17_lo, bit32_band(-1 - L18_lo, L19_lo)), bit32_bxor(L18_lo, bit32_band(-1 - L19_lo, L20_lo)), bit32_bxor(L19_lo, bit32_band(-1 - L20_lo, L16_lo))
			L16_hi, L17_hi, L18_hi, L19_hi, L20_hi = bit32_bxor(L20_hi, bit32_band(-1 - L16_hi, L17_hi)), bit32_bxor(L16_hi, bit32_band(-1 - L17_hi, L18_hi)), bit32_bxor(L17_hi, bit32_band(-1 - L18_hi, L19_hi)), bit32_bxor(L18_hi, bit32_band(-1 - L19_hi, L20_hi)), bit32_bxor(L19_hi, bit32_band(-1 - L20_hi, L16_hi))
			L21_lo, L22_lo, L23_lo, L24_lo, L25_lo = bit32_bxor(L23_lo, bit32_band(-1 - L24_lo, L25_lo)), bit32_bxor(L24_lo, bit32_band(-1 - L25_lo, L21_lo)), bit32_bxor(L25_lo, bit32_band(-1 - L21_lo, L22_lo)), bit32_bxor(L21_lo, bit32_band(-1 - L22_lo, L23_lo)), bit32_bxor(L22_lo, bit32_band(-1 - L23_lo, L24_lo))
			L21_hi, L22_hi, L23_hi, L24_hi, L25_hi = bit32_bxor(L23_hi, bit32_band(-1 - L24_hi, L25_hi)), bit32_bxor(L24_hi, bit32_band(-1 - L25_hi, L21_hi)), bit32_bxor(L25_hi, bit32_band(-1 - L21_hi, L22_hi)), bit32_bxor(L21_hi, bit32_band(-1 - L22_hi, L23_hi)), bit32_bxor(L22_hi, bit32_band(-1 - L23_hi, L24_hi))
			L01_lo = bit32_bxor(L01_lo, RC_lo[round_idx])
			L01_hi = L01_hi + RC_hi[round_idx] -- RC_hi[] is either 0 or 0x80000000, so we could use fast addition instead of slow XOR
		end

		lanes_lo[1] = L01_lo
		lanes_hi[1] = L01_hi
		lanes_lo[2] = L02_lo
		lanes_hi[2] = L02_hi
		lanes_lo[3] = L03_lo
		lanes_hi[3] = L03_hi
		lanes_lo[4] = L04_lo
		lanes_hi[4] = L04_hi
		lanes_lo[5] = L05_lo
		lanes_hi[5] = L05_hi
		lanes_lo[6] = L06_lo
		lanes_hi[6] = L06_hi
		lanes_lo[7] = L07_lo
		lanes_hi[7] = L07_hi
		lanes_lo[8] = L08_lo
		lanes_hi[8] = L08_hi
		lanes_lo[9] = L09_lo
		lanes_hi[9] = L09_hi
		lanes_lo[10] = L10_lo
		lanes_hi[10] = L10_hi
		lanes_lo[11] = L11_lo
		lanes_hi[11] = L11_hi
		lanes_lo[12] = L12_lo
		lanes_hi[12] = L12_hi
		lanes_lo[13] = L13_lo
		lanes_hi[13] = L13_hi
		lanes_lo[14] = L14_lo
		lanes_hi[14] = L14_hi
		lanes_lo[15] = L15_lo
		lanes_hi[15] = L15_hi
		lanes_lo[16] = L16_lo
		lanes_hi[16] = L16_hi
		lanes_lo[17] = L17_lo
		lanes_hi[17] = L17_hi
		lanes_lo[18] = L18_lo
		lanes_hi[18] = L18_hi
		lanes_lo[19] = L19_lo
		lanes_hi[19] = L19_hi
		lanes_lo[20] = L20_lo
		lanes_hi[20] = L20_hi
		lanes_lo[21] = L21_lo
		lanes_hi[21] = L21_hi
		lanes_lo[22] = L22_lo
		lanes_hi[22] = L22_hi
		lanes_lo[23] = L23_lo
		lanes_hi[23] = L23_hi
		lanes_lo[24] = L24_lo
		lanes_hi[24] = L24_hi
		lanes_lo[25] = L25_lo
		lanes_hi[25] = L25_hi
	end
end

--------------------------------------------------------------------------------
-- MAGIC NUMBERS CALCULATOR
--------------------------------------------------------------------------------
-- Q:
--    Is 53-bit "double" math enough to calculate square roots and cube roots of primes with 64 correct bits after decimal point?
-- A:
--    Yes, 53-bit "double" arithmetic is enough.
--    We could obtain first 40 bits by direct calculation of p^(1/3) and next 40 bits by one step of Newton's method.
do
	local function mul(src1, src2, factor, result_length)
		-- src1, src2 - long integers (arrays of digits in base TWO_POW_24)
		-- factor - small integer
		-- returns long integer result (src1 * src2 * factor) and its floating point approximation
		local result, carry, value, weight = table.create(result_length), 0, 0, 1
		for j = 1, result_length do
			for k = math.max(1, j + 1 - #src2), math.min(j, #src1) do
				carry = carry + factor * src1[k] * src2[j + 1 - k] -- "int32" is not enough for multiplication result, that's why "factor" must be of type "double"
			end

			local digit = carry % TWO_POW_24
			result[j] = math.floor(digit)
			carry = (carry - digit) / TWO_POW_24
			value = value + digit * weight
			weight = weight * TWO_POW_24
		end

		return result, value
	end

	local idx, step, p, one, sqrt_hi, sqrt_lo = 0, {4, 1, 2, -2, 2}, 4, {1}, sha2_H_hi, sha2_H_lo
	repeat
		p = p + step[p % 6]
		local d = 1
		repeat
			d = d + step[d % 6]
			if d * d > p then
				-- next prime number is found
				local root = p ^ (1 / 3)
				local R = root * TWO_POW_40
				R = mul(table.create(1, math.floor(R)), one, 1, 2)
				local _, delta = mul(R, mul(R, R, 1, 4), -1, 4)
				local hi = R[2] % 65536 * 65536 + math.floor(R[1] / 256)
				local lo = R[1] % 256 * 16777216 + math.floor(delta * (TWO_POW_NEG_56 / 3) * root / p)

				if idx < 16 then
					root = math.sqrt(p)
					R = root * TWO_POW_40
					R = mul(table.create(1, math.floor(R)), one, 1, 2)
					_, delta = mul(R, R, -1, 2)
					local hi = R[2] % 65536 * 65536 + math.floor(R[1] / 256)
					local lo = R[1] % 256 * 16777216 + math.floor(delta * TWO_POW_NEG_17 / root)
					local idx = idx % 8 + 1
					sha2_H_ext256[224][idx] = lo
					sqrt_hi[idx], sqrt_lo[idx] = hi, lo + hi * hi_factor
					if idx > 7 then
						sqrt_hi, sqrt_lo = sha2_H_ext512_hi[384], sha2_H_ext512_lo[384]
					end
				end

				idx = idx + 1
				sha2_K_hi[idx], sha2_K_lo[idx] = hi, lo % K_lo_modulo + hi * hi_factor
				break
			end
		until p % d == 0
	until idx > 79
end

-- Calculating IVs for SHA512/224 and SHA512/256
for width = 224, 256, 32 do
	local H_lo, H_hi = {}, nil
	if XOR64A5 then
		for j = 1, 8 do
			H_lo[j] = XOR64A5(sha2_H_lo[j])
		end
	else
		H_hi = {}
		for j = 1, 8 do
			H_lo[j] = bit32_bxor(sha2_H_lo[j], 0xA5A5A5A5) % 4294967296
			H_hi[j] = bit32_bxor(sha2_H_hi[j], 0xA5A5A5A5) % 4294967296
		end
	end

	sha512_feed_128(H_lo, H_hi, "SHA-512/" .. tostring(width) .. "\128" .. string.rep("\0", 115) .. "\88", 0, 128)
	sha2_H_ext512_lo[width] = H_lo
	sha2_H_ext512_hi[width] = H_hi
end

-- Constants for MD5
do
	for idx = 1, 64 do
		-- we can't use formula math.floor(abs(sin(idx))*TWO_POW_32) because its result may be beyond integer range on Lua built with 32-bit integers
		local hi, lo = math.modf(math.abs(math.sin(idx)) * TWO_POW_16)
		md5_K[idx] = hi * 65536 + math.floor(lo * TWO_POW_16)
	end
end

)__HASHUA__" +
R"__HASHUA__(
-- Constants for SHA3
do
	local sh_reg = 29
	local function next_bit()
		local r = sh_reg % 2
		sh_reg = bit32_bxor((sh_reg - r) / 2, 142 * r)
		return r
	end

	for idx = 1, 24 do
		local lo, m = 0, nil
		for _ = 1, 6 do
			m = m and m * m * 2 or 1
			lo = lo + next_bit() * m
		end

		local hi = next_bit() * m
		sha3_RC_hi[idx], sha3_RC_lo[idx] = hi, lo + hi * hi_factor_keccak
	end
end
)__HASHUA__" +
R"__HASHUA__(
local function sha256ext(width, message)
	-- Create an instance (private objects for current calculation)
	local Array256 = sha2_H_ext256[width] -- # == 8
	local length, tail = 0, ""
	local H = table.create(8)
	H[1], H[2], H[3], H[4], H[5], H[6], H[7], H[8] = Array256[1], Array256[2], Array256[3], Array256[4], Array256[5], Array256[6], Array256[7], Array256[8]

	local function partial(message_part)
		if message_part then
			local partLength = #message_part
			if tail then
				length = length + partLength
				local offs = 0
				local tailLength = #tail
				if tail ~= "" and tailLength + partLength >= 64 then
					offs = 64 - tailLength
					sha256_feed_64(H, tail .. string.sub(message_part, 1, offs), 0, 64)
					tail = ""
				end

				local size = partLength - offs
				local size_tail = size % 64
				sha256_feed_64(H, message_part, offs, size - size_tail)
				tail = tail .. string.sub(message_part, partLength + 1 - size_tail)
				return partial
			else
				error("Adding more chunks is not allowed after receiving the result", 2)
			end
		else
			if tail then
				local final_blocks = table.create(10) --{tail, "\128", string.rep("\0", (-9 - length) % 64 + 1)}
				final_blocks[1] = tail
				final_blocks[2] = "\128"
				final_blocks[3] = string.rep("\0", (-9 - length) % 64 + 1)

				tail = nil
				-- Assuming user data length is shorter than (TWO_POW_53)-9 bytes
				-- Anyway, it looks very unrealistic that someone would spend more than a year of calculations to process TWO_POW_53 bytes of data by using this Lua script :-)
				-- TWO_POW_53 bytes = TWO_POW_56 bits, so "bit-counter" fits in 7 bytes
				length = length * (8 / TWO56_POW_7) -- convert "byte-counter" to "bit-counter" and move decimal point to the left
				for j = 4, 10 do
					length = length % 1 * 256
					final_blocks[j] = string.char(math.floor(length))
				end

				final_blocks = table.concat(final_blocks)
				sha256_feed_64(H, final_blocks, 0, #final_blocks)
				local max_reg = width / 32
				for j = 1, max_reg do
					H[j] = string.format("%08x", H[j] % 4294967296)
				end

				H = table.concat(H, "", 1, max_reg)
			end

			return H
		end
	end

	if message then
		-- Actually perform calculations and return the SHA256 digest of a message
		return partial(message)()
	else
		-- Return function for chunk-by-chunk loading
		-- User should feed every chunk of input data as single argument to this function and finally get SHA256 digest by invoking this function without an argument
		return partial
	end
end

local function sha512ext(width, message)

	-- Create an instance (private objects for current calculation)
	local length, tail, H_lo, H_hi = 0, "", table.pack(table.unpack(sha2_H_ext512_lo[width])), not HEX64 and table.pack(table.unpack(sha2_H_ext512_hi[width]))

	local function partial(message_part)
		if message_part then
			local partLength = #message_part
			if tail then
				length = length + partLength
				local offs = 0
				if tail ~= "" and #tail + partLength >= 128 then
					offs = 128 - #tail
					sha512_feed_128(H_lo, H_hi, tail .. string.sub(message_part, 1, offs), 0, 128)
					tail = ""
				end

				local size = partLength - offs
				local size_tail = size % 128
				sha512_feed_128(H_lo, H_hi, message_part, offs, size - size_tail)
				tail = tail .. string.sub(message_part, partLength + 1 - size_tail)
				return partial
			else
				error("Adding more chunks is not allowed after receiving the result", 2)
			end
		else
			if tail then
				local final_blocks = table.create(3) --{tail, "\128", string.rep("\0", (-17-length) % 128 + 9)}
				final_blocks[1] = tail
				final_blocks[2] = "\128"
				final_blocks[3] = string.rep("\0", (-17 - length) % 128 + 9)

				tail = nil
				-- Assuming user data length is shorter than (TWO_POW_53)-17 bytes
				-- TWO_POW_53 bytes = TWO_POW_56 bits, so "bit-counter" fits in 7 bytes
				length = length * (8 / TWO56_POW_7) -- convert "byte-counter" to "bit-counter" and move floating point to the left
				for j = 4, 10 do
					length = length % 1 * 256
					final_blocks[j] = string.char(math.floor(length))
				end

				final_blocks = table.concat(final_blocks)
				sha512_feed_128(H_lo, H_hi, final_blocks, 0, #final_blocks)
				local max_reg = math.ceil(width / 64)

				if HEX64 then
					for j = 1, max_reg do
						H_lo[j] = HEX64(H_lo[j])
					end
				else
					for j = 1, max_reg do
						H_lo[j] = string.format("%08x", H_hi[j] % 4294967296) .. string.format("%08x", H_lo[j] % 4294967296)
					end

					H_hi = nil
				end

				H_lo = string.sub(table.concat(H_lo, "", 1, max_reg), 1, width / 4)
			end

			return H_lo
		end
	end

	if message then
		-- Actually perform calculations and return the SHA512 digest of a message
		return partial(message)()
	else
		-- Return function for chunk-by-chunk loading
		-- User should feed every chunk of input data as single argument to this function and finally get SHA512 digest by invoking this function without an argument
		return partial
	end
end

local function md5(message)

	-- Create an instance (private objects for current calculation)
	local H, length, tail = table.create(4), 0, ""
	H[1], H[2], H[3], H[4] = md5_sha1_H[1], md5_sha1_H[2], md5_sha1_H[3], md5_sha1_H[4]

	local function partial(message_part)
		if message_part then
			local partLength = #message_part
			if tail then
				length = length + partLength
				local offs = 0
				if tail ~= "" and #tail + partLength >= 64 then
					offs = 64 - #tail
					md5_feed_64(H, tail .. string.sub(message_part, 1, offs), 0, 64)
					tail = ""
				end

				local size = partLength - offs
				local size_tail = size % 64
				md5_feed_64(H, message_part, offs, size - size_tail)
				tail = tail .. string.sub(message_part, partLength + 1 - size_tail)
				return partial
			else
				error("Adding more chunks is not allowed after receiving the result", 2)
			end
		else
			if tail then
				local final_blocks = table.create(3) --{tail, "\128", string.rep("\0", (-9 - length) % 64)}
				final_blocks[1] = tail
				final_blocks[2] = "\128"
				final_blocks[3] = string.rep("\0", (-9 - length) % 64)
				tail = nil
				length = length * 8 -- convert "byte-counter" to "bit-counter"
				for j = 4, 11 do
					local low_byte = length % 256
					final_blocks[j] = string.char(low_byte)
					length = (length - low_byte) / 256
				end

				final_blocks = table.concat(final_blocks)
				md5_feed_64(H, final_blocks, 0, #final_blocks)
				for j = 1, 4 do
					H[j] = string.format("%08x", H[j] % 4294967296)
				end

				H = string.gsub(table.concat(H), "(..)(..)(..)(..)", "%4%3%2%1")
			end

			return H
		end
	end

	if message then
		-- Actually perform calculations and return the MD5 digest of a message
		return partial(message)()
	else
		-- Return function for chunk-by-chunk loading
		-- User should feed every chunk of input data as single argument to this function and finally get MD5 digest by invoking this function without an argument
		return partial
	end
end

local function sha1(message)
	-- Create an instance (private objects for current calculation)
	local H, length, tail = table.pack(table.unpack(md5_sha1_H)), 0, ""

	local function partial(message_part)
		if message_part then
			local partLength = #message_part
			if tail then
				length = length + partLength
				local offs = 0
				if tail ~= "" and #tail + partLength >= 64 then
					offs = 64 - #tail
					sha1_feed_64(H, tail .. string.sub(message_part, 1, offs), 0, 64)
					tail = ""
				end

				local size = partLength - offs
				local size_tail = size % 64
				sha1_feed_64(H, message_part, offs, size - size_tail)
				tail = tail .. string.sub(message_part, partLength + 1 - size_tail)
				return partial
			else
				error("Adding more chunks is not allowed after receiving the result", 2)
			end
		else
			if tail then
				local final_blocks = table.create(10) --{tail, "\128", string.rep("\0", (-9 - length) % 64 + 1)}
				final_blocks[1] = tail
				final_blocks[2] = "\128"
				final_blocks[3] = string.rep("\0", (-9 - length) % 64 + 1)
				tail = nil

				-- Assuming user data length is shorter than (TWO_POW_53)-9 bytes
				-- TWO_POW_53 bytes = TWO_POW_56 bits, so "bit-counter" fits in 7 bytes
				length = length * (8 / TWO56_POW_7) -- convert "byte-counter" to "bit-counter" and move decimal point to the left
				for j = 4, 10 do
					length = length % 1 * 256
					final_blocks[j] = string.char(math.floor(length))
				end

				final_blocks = table.concat(final_blocks)
				sha1_feed_64(H, final_blocks, 0, #final_blocks)
				for j = 1, 5 do
					H[j] = string.format("%08x", H[j] % 4294967296)
				end

				H = table.concat(H)
			end

			return H
		end
	end

	if message then
		-- Actually perform calculations and return the SHA-1 digest of a message
		return partial(message)()
	else
		-- Return function for chunk-by-chunk loading
		-- User should feed every chunk of input data as single argument to this function and finally get SHA-1 digest by invoking this function without an argument
		return partial
	end
end

local function keccak(block_size_in_bytes, digest_size_in_bytes, is_SHAKE, message)
	-- "block_size_in_bytes" is multiple of 8
	if type(digest_size_in_bytes) ~= "number" then
		-- arguments in SHAKE are swapped:
		--    NIST FIPS 202 defines SHAKE(message,num_bits)
		--    this module   defines SHAKE(num_bytes,message)
		-- it's easy to forget about this swap, hence the check
		error("Argument 'digest_size_in_bytes' must be a number", 2)
	end

	-- Create an instance (private objects for current calculation)
	local tail, lanes_lo, lanes_hi = "", table.create(25, 0), hi_factor_keccak == 0 and table.create(25, 0)
	local result

	--~     pad the input N using the pad function, yielding a padded bit string P with a length divisible by r (such that n = len(P)/r is integer),
	--~     break P into n consecutive r-bit pieces P0, ..., Pn-1 (last is zero-padded)
	--~     initialize the state S to a string of b 0 bits.
	--~     absorb the input into the state: For each block Pi,
	--~         extend Pi at the end by a string of c 0 bits, yielding one of length b,
	--~         XOR that with S and
	--~         apply the block permutation f to the result, yielding a new state S
	--~     initialize Z to be the empty string
	--~     while the length of Z is less than d:
	--~         append the first r bits of S to Z
	--~         if Z is still less than d bits long, apply f to S, yielding a new state S.
	--~     truncate Z to d bits
	local function partial(message_part)
		if message_part then
			local partLength = #message_part
			if tail then
				local offs = 0
				if tail ~= "" and #tail + partLength >= block_size_in_bytes then
					offs = block_size_in_bytes - #tail
)__HASHUA__" +
R"__HASHUA__(
					keccak_feed(lanes_lo, lanes_hi, tail .. string.sub(message_part, 1, offs), 0, block_size_in_bytes, block_size_in_bytes)
					tail = ""
				end

				local size = partLength - offs
				local size_tail = size % block_size_in_bytes
				keccak_feed(lanes_lo, lanes_hi, message_part, offs, size - size_tail, block_size_in_bytes)
				tail = tail .. string.sub(message_part, partLength + 1 - size_tail)
				return partial
			else
				error("Adding more chunks is not allowed after receiving the result", 2)
			end
		else
			if tail then
				-- append the following bits to the message: for usual SHA3: 011(0*)1, for SHAKE: 11111(0*)1
				local gap_start = is_SHAKE and 31 or 6
				tail = tail .. (#tail + 1 == block_size_in_bytes and string.char(gap_start + 128) or string.char(gap_start) .. string.rep("\0", (-2 - #tail) % block_size_in_bytes) .. "\128")
				keccak_feed(lanes_lo, lanes_hi, tail, 0, #tail, block_size_in_bytes)
				tail = nil

				local lanes_used = 0
				local total_lanes = math.floor(block_size_in_bytes / 8)
				local qwords = {}

				local function get_next_qwords_of_digest(qwords_qty)
					-- returns not more than 'qwords_qty' qwords ('qwords_qty' might be non-integer)
					-- doesn't go across keccak-buffer boundary
					-- block_size_in_bytes is a multiple of 8, so, keccak-buffer contains integer number of qwords
					if lanes_used >= total_lanes then
						keccak_feed(lanes_lo, lanes_hi, "\0\0\0\0\0\0\0\0", 0, 8, 8)
						lanes_used = 0
					end

					qwords_qty = math.floor(math.min(qwords_qty, total_lanes - lanes_used))
					if hi_factor_keccak ~= 0 then
						for j = 1, qwords_qty do
							qwords[j] = HEX64(lanes_lo[lanes_used + j - 1 + lanes_index_base])
						end
					else
						for j = 1, qwords_qty do
							qwords[j] = string.format("%08x", lanes_hi[lanes_used + j] % 4294967296) .. string.format("%08x", lanes_lo[lanes_used + j] % 4294967296)
						end
					end

					lanes_used = lanes_used + qwords_qty
					return string.gsub(table.concat(qwords, "", 1, qwords_qty), "(..)(..)(..)(..)(..)(..)(..)(..)", "%8%7%6%5%4%3%2%1"), qwords_qty * 8
				end

				local parts = {} -- digest parts
				local last_part, last_part_size = "", 0

				local function get_next_part_of_digest(bytes_needed)
					-- returns 'bytes_needed' bytes, for arbitrary integer 'bytes_needed'
					bytes_needed = bytes_needed or 1
					if bytes_needed <= last_part_size then
						last_part_size = last_part_size - bytes_needed
						local part_size_in_nibbles = bytes_needed * 2
						local result = string.sub(last_part, 1, part_size_in_nibbles)
						last_part = string.sub(last_part, part_size_in_nibbles + 1)
						return result
					end

					local parts_qty = 0
					if last_part_size > 0 then
						parts_qty = 1
						parts[parts_qty] = last_part
						bytes_needed = bytes_needed - last_part_size
					end

					-- repeats until the length is enough
					while bytes_needed >= 8 do
						local next_part, next_part_size = get_next_qwords_of_digest(bytes_needed / 8)
						parts_qty = parts_qty + 1
						parts[parts_qty] = next_part
						bytes_needed = bytes_needed - next_part_size
					end

					if bytes_needed > 0 then
						last_part, last_part_size = get_next_qwords_of_digest(1)
						parts_qty = parts_qty + 1
						parts[parts_qty] = get_next_part_of_digest(bytes_needed)
					else
						last_part, last_part_size = "", 0
					end

					return table.concat(parts, "", 1, parts_qty)
				end

				if digest_size_in_bytes < 0 then
					result = get_next_part_of_digest
				else
					result = get_next_part_of_digest(digest_size_in_bytes)
				end

			end

			return result
		end
	end

	if message then
		-- Actually perform calculations and return the SHA3 digest of a message
		return partial(message)()
	else
		-- Return function for chunk-by-chunk loading
		-- User should feed every chunk of input data as single argument to this function and finally get SHA3 digest by invoking this function without an argument
		return partial
	end
end

local function HexToBinFunction(hh)
	return string.char(tonumber(hh, 16))
end

local function hex2bin(hex_string)
	return (string.gsub(hex_string, "%x%x", HexToBinFunction))
end

local base64_symbols = {
	["+"] = 62, ["-"] = 62, [62] = "+";
	["/"] = 63, ["_"] = 63, [63] = "/";
	["="] = -1, ["."] = -1, [-1] = "=";
}

local symbol_index = 0
for j, pair in ipairs{"AZ", "az", "09"} do
	for ascii = string.byte(pair), string.byte(pair, 2) do
		local ch = string.char(ascii)
		base64_symbols[ch] = symbol_index
		base64_symbols[symbol_index] = ch
		symbol_index = symbol_index + 1
	end
end

local function bin2base64(binary_string)
	local stringLength = #binary_string
	local result = table.create(math.ceil(stringLength / 3))
	local length = 0

	for pos = 1, #binary_string, 3 do
		local c1, c2, c3, c4 = string.byte(string.sub(binary_string, pos, pos + 2) .. '\0', 1, -1)
		length = length + 1
		result[length] =
			base64_symbols[math.floor(c1 / 4)] ..
			base64_symbols[c1 % 4 * 16 + math.floor(c2 / 16)] ..
			base64_symbols[c3 and c2 % 16 * 4 + math.floor(c3 / 64) or -1] ..
			base64_symbols[c4 and c3 % 64 or -1]
	end

	return table.concat(result)
end

local function base642bin(base64_string)
	local result, chars_qty = {}, 3
	for pos, ch in string.gmatch(string.gsub(base64_string, "%s+", ""), "()(.)") do
		local code = base64_symbols[ch]
		if code < 0 then
			chars_qty = chars_qty - 1
			code = 0
		end

		local idx = pos % 4
		if idx > 0 then
			result[-idx] = code
		else
			local c1 = result[-1] * 4 + math.floor(result[-2] / 16)
			local c2 = (result[-2] % 16) * 16 + math.floor(result[-3] / 4)
			local c3 = (result[-3] % 4) * 64 + code
			result[#result + 1] = string.sub(string.char(c1, c2, c3), 1, chars_qty)
		end
	end

	return table.concat(result)
end

local block_size_for_HMAC -- this table will be initialized at the end of the module
--local function pad_and_xor(str, result_length, byte_for_xor)
--	return string.gsub(str, ".", function(c)
--		return string.char(bit32_bxor(string.byte(c), byte_for_xor))
--	end) .. string.rep(string.char(byte_for_xor), result_length - #str)
--end

-- For the sake of speed of converting hexes to strings, there's a map of the conversions here
local BinaryStringMap = {}
for Index = 0, 255 do
	BinaryStringMap[string.format("%02x", Index)] = string.char(Index)
end

-- Update 02.14.20 - added AsBinary for easy GameAnalytics replacement.
local function hmac(hash_func, key, message, AsBinary)
	-- Create an instance (private objects for current calculation)
	local block_size = block_size_for_HMAC[hash_func]
	if not block_size then
		error("Unknown hash function", 2)
	end

	local KeyLength = #key
	if KeyLength > block_size then
		key = string.gsub(hash_func(key), "%x%x", HexToBinFunction)
		KeyLength = #key
	end

	local append = hash_func()(string.gsub(key, ".", function(c)
		return string.char(bit32_bxor(string.byte(c), 0x36))
	end) .. string.rep("6", block_size - KeyLength)) -- 6 = string.char(0x36)

	local result

	local function partial(message_part)
		if not message_part then
			result = result or hash_func(
				string.gsub(key, ".", function(c)
					return string.char(bit32_bxor(string.byte(c), 0x5c))
				end) .. string.rep("\\", block_size - KeyLength) -- \ = string.char(0x5c)
				.. (string.gsub(append(), "%x%x", HexToBinFunction))
			)

			return result
		elseif result then
			error("Adding more chunks is not allowed after receiving the result", 2)
		else
			append(message_part)
			return partial
		end
	end

	if message then
		-- Actually perform calculations and return the HMAC of a message
		local FinalMessage = partial(message)()
		return AsBinary and (string.gsub(FinalMessage, "%x%x", BinaryStringMap)) or FinalMessage
	else
		-- Return function for chunk-by-chunk loading of a message
		-- User should feed every chunk of the message as single argument to this function and finally get HMAC by invoking this function without an argument
		return partial
	end
end

local sha = {
	md5 = md5,
	sha1 = sha1,
	-- SHA2 hash functions:
	sha224 = function(message)
		return sha256ext(224, message)
	end;

	sha256 = function(message)
		return sha256ext(256, message)
	end;

	sha512_224 = function(message)
		return sha512ext(224, message)
	end;

	sha512_256 = function(message)
		return sha512ext(256, message)
	end;

	sha384 = function(message)
		return sha512ext(384, message)
	end;

	sha512 = function(message)
		return sha512ext(512, message)
	end;

	-- SHA3 hash functions:
	sha3_224 = function(message)
		return keccak((1600 - 2 * 224) / 8, 224 / 8, false, message)
	end;

	sha3_256 = function(message)
		return keccak((1600 - 2 * 256) / 8, 256 / 8, false, message)
	end;

	sha3_384 = function(message)
		return keccak((1600 - 2 * 384) / 8, 384 / 8, false, message)
	end;

	sha3_512 = function(message)
		return keccak((1600 - 2 * 512) / 8, 512 / 8, false, message)
	end;

	shake128 = function(message, digest_size_in_bytes)
		return keccak((1600 - 2 * 128) / 8, digest_size_in_bytes, true, message)
	end;

	shake256 = function(message, digest_size_in_bytes)
		return keccak((1600 - 2 * 256) / 8, digest_size_in_bytes, true, message)
	end;

	-- misc utilities:
	hmac = hmac; -- HMAC(hash_func, key, message) is applicable to any hash function from this module except SHAKE*
	hex_to_bin = hex2bin; -- converts hexadecimal representation to binary string
	base64_to_bin = base642bin; -- converts base64 representation to binary string
	bin_to_base64 = bin2base64; -- converts binary string to base64 representation
	base64_encode = Base64.Encode;
	base64_decode = Base64.Decode;
}

block_size_for_HMAC = {
	[sha.md5] = 64;
	[sha.sha1] = 64;
	[sha.sha224] = 64;
	[sha.sha256] = 64;
	[sha.sha512_224] = 128;
	[sha.sha512_256] = 128;
	[sha.sha384] = 128;
	[sha.sha512] = 128;
	[sha.sha3_224] = (1600 - 2 * 224) / 8;
	[sha.sha3_256] = (1600 - 2 * 256) / 8;
	[sha.sha3_384] = (1600 - 2 * 384) / 8;
	[sha.sha3_512] = (1600 - 2 * 512) / 8;
}

return sha
)__HASHUA__";



std::string GetLuaCode(DWORD pid, int idx) {

    HMODULE hModule = NULL;

    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCWSTR)&GetLuaCode, &hModule);



    HRSRC resourceHandle = FindResourceW(hModule, MAKEINTRESOURCEW(idx), RT_RCDATA);

    if (resourceHandle == NULL)

    {

        return "";

    }



    HGLOBAL loadedResource = LoadResource(hModule, resourceHandle);

    if (loadedResource == NULL)

    {

        return "";

    }



    DWORD size = SizeofResource(hModule, resourceHandle);

    void* data = LockResource(loadedResource);



    std::string code = std::string(static_cast<char*>(data), size);

    size_t pos = code.find("%-PROCESS-ID-%");

    if (pos != std::string::npos) {

        code.replace(pos, 14, std::to_string(pid));

    }



    return code;

}

void decompiler()

{

    std::filesystem::path folder = std::filesystem::current_path() / "bin";

    std::filesystem::create_directories(folder);

    std::filesystem::path exePath = folder / "server.exe";



    if (!std::filesystem::exists(exePath))

    {

        const char* url = "https://raw.githubusercontent.com/HyperCraxy/RKOAPI/main/server.exe";

        HRESULT hr = URLDownloadToFileA(nullptr, url, exePath.string().c_str(), 0, nullptr);

        if (FAILED(hr)) return;

    }



    STARTUPINFOA si{};

    PROCESS_INFORMATION pi{};

    si.cb = sizeof(si);

    si.dwFlags = STARTF_USESHOWWINDOW;

    si.wShowWindow = SW_HIDE;

    if (CreateProcessA(nullptr, const_cast<char*>(exePath.string().c_str()), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi))

    {

        CloseHandle(pi.hThread);

        CloseHandle(pi.hProcess);

    }



    std::thread([] {

        HANDLE self = OpenProcess(SYNCHRONIZE, FALSE, GetCurrentProcessId());

        if (self) {

            WaitForSingleObject(self, INFINITE);

            CloseHandle(self);

        }

        }).detach();

}

int main()

{

    printf("[*] starting bridge thread\n");

    std::thread(StartBridge).detach();

    printf("[*] starting decompiler server");

    std::thread(decompiler).detach();

    printf("[*] getting process list\n");

    std::vector<DWORD> pids = Process::GetProcessID();

    printf("[+] found %zu processes\n", pids.size());



    for (DWORD pid : pids) {

        printf("[*] checking process %lu\n", pid);



        uintptr_t base = Process::GetModuleBase(pid);

        if (!base) {

            printf("[-] failed to get module base for pid %lu\n", pid);

            continue;

        }

        printf("[+] got module base: 0x%p\n", (void*)base);



        Instance Datamodel = FetchDatamodel(base, pid);

        if (!Datamodel.GetAddress()) {

            printf("[-] failed to fetch datamodel for pid %lu\n", pid);

            continue;

        }

        printf("[+] datamodel name: %s\n", Datamodel.Name().c_str());



        size_t inits;

        std::string initLua = GetLuaCode(pid, 1);

        std::vector<char> initb = Bytecode::Sign(Bytecode::Compile(initLua), inits);

        printf("[+] compiled init script, size: %zu\n", inits);



        if (Datamodel.Name() == "Ugc") {

            printf("[*] handling ugc datamodel\n");



            printf("[*] finding coregui\n");

            Instance CoreGui = Datamodel.FindFirstChild("CoreGui");

            if (!CoreGui.GetAddress()) {

                printf("[-] coregui not found\n");

                continue;

            }



            printf("[*] finding robloxgui\n");

            Instance RobloxGui = CoreGui.FindFirstChild("RobloxGui");

            if (!RobloxGui.GetAddress()) {

                printf("[-] robloxgui not found\n");

                continue;

            }



            printf("[*] finding modules\n");

            Instance Modules = RobloxGui.FindFirstChild("Modules");

            if (!Modules.GetAddress()) {

                printf("[-] modules not found\n");

                continue;

            }



            printf("[*] finding playerlist\n");

            Instance PlayerList = Modules.FindFirstChild("PlayerList");

            if (!PlayerList.GetAddress()) {

                printf("[-] playerlist not found\n");

                continue;

            }



            printf("[*] finding playerlistmanager\n");

            Instance PlmModule = PlayerList.FindFirstChild("PlayerListManager");

            if (!PlmModule.GetAddress()) {

                printf("[-] playerlistmanager not found\n");

                continue;

            }



            printf("[*] finding collision matchers\n");

            Instance CorePackages = Datamodel.FindFirstChild("CorePackages");

            Instance Packages = CorePackages.FindFirstChild("Packages");

            Instance Index = Packages.FindFirstChild("_Index");

            Instance Cm2D1 = Index.FindFirstChild("CollisionMatchers2D");

            Instance Cm2D2 = Cm2D1.FindFirstChild("CollisionMatchers2D");

            Instance Jest = Cm2D2.FindFirstChild("Jest");



            if (!Jest.GetAddress()) {

                printf("[-] jest module not found\n");

                continue;

            }



            printf("[*] enabling load module\n");

            WriteMemory<BYTE>(base + offsets::EnableLoadModule, 1, pid);



            printf("[*] hijacking playerlistmanager\n");

            WriteMemory<uintptr_t>(PlmModule.GetAddress() + 0x8, Jest.GetAddress(), pid);



            printf("[*] setting jest bytecode\n");

            auto revert1 = Jest.SetScriptBytecode(initb, inits);



            printf("[*] bringing window to foreground\n");

            HWND hwnd = Process::GetWindowsProcess(pid);

            HWND old = GetForegroundWindow();

            while (GetForegroundWindow() != hwnd) {

                SetForegroundWindow(hwnd);

                Sleep(1);

            }



            printf("[*] sending escape key\n");

            keybd_event(VK_ESCAPE, MapVirtualKey(VK_ESCAPE, 0), KEYEVENTF_SCANCODE, 0);

            keybd_event(VK_ESCAPE, MapVirtualKey(VK_ESCAPE, 0), KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP, 0);



            printf("[*] waiting for RKO gui\n");

            CoreGui.WaitForChild("RKO");

            printf("[+] RKO gui loaded\n");

            Sleep(250);  // Wait 50 milliseconds
            keybd_event(VK_ESCAPE, MapVirtualKey(VK_ESCAPE, 0), KEYEVENTF_SCANCODE, 0);
            keybd_event(VK_ESCAPE, MapVirtualKey(VK_ESCAPE, 0), KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP, 0);


            printf("[*] restoring foreground window\n");

            SetForegroundWindow(old);



            printf("[*] restoring playerlistmanager\n");

            WriteMemory<uintptr_t>(PlmModule.GetAddress() + 0x8, PlmModule.GetAddress(), pid);



            printf("[*] reverting bytecode\n");

            revert1();

            printf("[+] ugc injection complete\n");



        }

        else {

            printf("[*] setting up watcher for ugc datamodel\n");

            std::thread([=]() {

                Instance Datamodel = Instance(0, pid);

                int attempts = 0;

                while (true) {

                    Datamodel = FetchDatamodel(base, pid);

                    if (Datamodel.Name() == "Ugc") {

                        printf("[+] ugc datamodel found after %d attempts\n", attempts);

                        break;

                    }

                    attempts++;

                    if (attempts % 10 == 0) {

                        printf("[*] still waiting for ugc datamodel... (%d attempts)\n", attempts);

                    }

                    Sleep(250);

                }



                printf("[*] finding coregui\n");

                Instance CoreGui = Datamodel.FindFirstChild("CoreGui");

                if (!CoreGui.GetAddress()) {

                    printf("[-] coregui not found\n");

                    return;

                }



                printf("[*] finding robloxgui\n");

                Instance RobloxGui = CoreGui.FindFirstChild("RobloxGui");

                if (!RobloxGui.GetAddress()) {

                    printf("[-] robloxgui not found\n");

                    return;

                }



                printf("[*] finding modules\n");

                Instance Modules = RobloxGui.FindFirstChild("Modules");

                if (!Modules.GetAddress()) {

                    printf("[-] modules not found\n");

                    return;

                }



                printf("[*] finding avatareditorprompts\n");

                Instance InitModule = Modules.FindFirstChild("AvatarEditorPrompts");

                if (!InitModule.GetAddress()) {

                    printf("[-] avatareditorprompts not found\n");

                    return;

                }



                printf("[*] enabling load module\n");

                WriteMemory<BYTE>(base + offsets::EnableLoadModule, 1, pid);



                printf("[*] setting init module bytecode\n");

                auto revert1 = InitModule.SetScriptBytecode(initb, inits);



                printf("[*] waiting for RKO gui\n");

                CoreGui.WaitForChild("RKO");

                printf("[+] RKO gui loaded\n");



                printf("[*] reverting bytecode\n");

                revert1();

                printf("[+] non-ugc injection complete\n");

                }).detach();

        }

    }



    printf("[*] main loop finished\n");

    return 0;

}



std::string Convert(const wchar_t* wideStr) {

    if (!wideStr) return "";

    int size_needed = WideCharToMultiByte(

        CP_UTF8, 0, wideStr, -1, nullptr, 0, nullptr, nullptr

    );

    if (size_needed == 0) return "";

    std::string result(size_needed, 0);

    WideCharToMultiByte(

        CP_UTF8, 0, wideStr, -1, &result[0], size_needed, nullptr, nullptr

    );

    if (!result.empty() && result.back() == '\0') {

        result.pop_back();

    }

    return result;

}



static bool Injected = false;

// First function: Handles injection
extern "C" __declspec(dllexport) void inj(const wchar_t* input) {
	if (!Injected) {
		Injected = true;
		main(); // Or whatever initialization function you need
	}
}

// Second function: Handles execution
extern "C" __declspec(dllexport) void execute(const wchar_t* input) {
	std::string source = Convert(input);
	if (source.length() >= 1) {
		Execute(source);
	}
}
