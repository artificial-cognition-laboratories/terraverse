-- What can be made from what's in reach: every made form whose first input's
-- there in some material, each with what it takes and, when it can't be made
-- yet, why. The same answer decides what's shown and what's made, so a make is
-- checked again, from scratch, when it happens.
local M, P, FORMS, BY_ID = ...

local function label(form, material)
	return (form.name:gsub("%%s", M[material].name))
end

local function item_of(form_id, material)
	return "tf_craft:" .. form_id .. "_" .. material
end

-- What's held, as counts by item and by form.
local function tally(stacks, items)
	local have = {}
	for _, stack in ipairs(stacks) do
		if not stack:is_empty() and items[stack:get_name()] then
			have[stack:get_name()] = (have[stack:get_name()] or 0) + stack:get_count()
		end
	end
	return have
end

-- Takes `n` of a form from what's left, the given material's if one's given;
-- returns what it took, or nil and how many there were.
local function pick(left, items, form_id, material, n)
	local took, total = {}, 0
	for name, count in pairs(left) do
		local it = items[name]
		if count > 0 and it.form == form_id and (not material or it.material == material) then
			local t = math.min(count, n - total)
			took[#took + 1] = { name = name, count = t }
			total = total + t
			if total == n then break end
		end
	end
	if total < n then return nil, total end
	for _, t in ipairs(took) do left[t.name] = left[t.name] - t.count end
	return took
end

local function offer(form, way, material, have, items, station)
	local process = P[way.by]
	local o = { id = form.id .. "_" .. material, form = form.id, label = label(form, material), count = way.count or 1,
		process = process.name, inputs = {}, ok = true }
	o.item = items[item_of(form.id, material)] and item_of(form.id, material) or nil
	local function refuse(why)
		if o.ok then o.ok, o.reason = false, why end
	end
	for prop, why in pairs(process.needs or {}) do
		if not M[material][prop] then refuse(M[material].name .. ": " .. why) end
	end
	local left, take = table.copy(have), {}
	for i, input in ipairs(way.from) do
		local of = BY_ID[input[1]]
		local need = input[2]
		local took, had = pick(left, items, of.id, i == 1 and material or nil, need)
		local named = i == 1 and label(of, material) or (of.name:gsub("%%s ?", ""))
		o.inputs[#o.inputs + 1] = { label = named, count = need, have = took and need or had }
		if took then
			for _, t in ipairs(took) do take[#take + 1] = t end
		else
			refuse(("needs %d %s, has %d"):format(need, named:lower(), had))
		end
	end
	if process.uses then
		local tool
		for name, count in pairs(left) do
			local it = items[name]
			local gives = count > 0 and BY_ID[it.form].gives or {}
			if table.indexof(gives, process.uses) > 0
					and (not process.harder or M[it.material].hardness > M[material].hardness) then
				tool = name
				break
			end
		end
		if tool then
			o.tool = items[tool].label
		else
			refuse(process.harder
				and ("needs a blade harder than %s"):format(M[material].name:lower())
				or ("needs something to %s with"):format(process.uses))
		end
	end
	if process.at and not (station and station.kind == process.at) then
		refuse("needs a " .. process.at)
	elseif not station and not process.hand then
		refuse("needs a workbench")
	end
	o.seconds = process.seconds * (station and station.quicker or process.slower)
	return o, take
end

-- Everything offered here, and for each the stacks a make takes (by offer id).
return function(stacks, items, station)
	local have = tally(stacks, items)
	local offers, takes = {}, {}
	for _, form in ipairs(FORMS) do
		local seen = {}
		for _, way in ipairs(form.made or {}) do
			local first = way.from[1][1]
			for name, count in pairs(have) do
				local it = items[name]
				-- (a material another way works first is that way's)
				if count > 0 and it.form == first and not seen[it.material]
						and (not form.way_of[it.material] or form.way_of[it.material] == way) then
					seen[it.material] = true
					local o, take = offer(form, way, it.material, have, items, station)
					offers[#offers + 1] = o
					takes[o.id] = take
				end
			end
		end
	end
	-- what can be made first, each group in the forms' order
	local order = {}
	for i, f in ipairs(FORMS) do order[f.id] = i end
	table.sort(offers, function(a, b)
		if a.ok ~= b.ok then return a.ok end
		if a.form ~= b.form then return order[a.form] < order[b.form] end
		return a.id < b.id
	end)
	return offers, takes
end
