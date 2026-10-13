-- parse_user_alg_config_mk.lua: 检查算法功能配置(system/user_alg_config.mk)相对 BNPU 工程构建快照(config.a)是否变化
--
-- 用法:
--   lua parse_user_alg_config_mk.lua check <user_alg_config.mk> <config快照文件>
--       比较两者所有宏的值: 完全一致退出码返回 0(无需重建 BNPU 工程);
--       只要有任何一个不一样, 退出码返回 1(不修改快照文件)
--   lua parse_user_alg_config_mk.lua sync  <user_alg_config.mk> <config快照文件>
--       把 user_alg_config.mk 中所有宏按原顺序以 key=value 形式写入快照文件
-- 退出码 2 表示参数错误或文件无法解析
--
-- 说明: 解析宏的值时, 若值中含数学运算符号(+、-、*、/)且能解析为整数算术表达式,
--       则直接求值并只保留计算结果(如 30*1024 -> 30720), 不再保留计算式

-- 把值当作整数算术表达式求值(支持十进制/0x十六进制整数、+ - * /、括号、一元正负号)
-- 表达式非法(含其它字符/语法错误/除零)时返回 nil
local function eval_math_expression(expr)
    local pos = 1

    local parse_expr  -- 前向声明: parse_factor 解析括号时需递归调用加减表达式

    -- 跳过空白字符
    local function skip_spaces()
        local _, e = expr:find("^%s*", pos)
        pos = e + 1
    end

    -- 解析因子: 数字 / 括号表达式 / 一元正负号
    local function parse_factor()
        skip_spaces()
        local c = expr:sub(pos, pos)
        if c == "+" then
            pos = pos + 1
            return parse_factor()
        elseif c == "-" then
            pos = pos + 1
            local value = parse_factor()
            return value and -value
        elseif c == "(" then
            pos = pos + 1
            local value = parse_expr()
            skip_spaces()
            if value == nil or expr:sub(pos, pos) ~= ")" then
                return nil
            end
            pos = pos + 1
            return value
        end
        -- 0x 十六进制整数
        local s, e, num_str = expr:find("^(0[xX]%x+)", pos)
        if not num_str then
            -- 十进制整数
            s, e, num_str = expr:find("^(%d+)", pos)
        end
        if not num_str then
            return nil
        end
        pos = e + 1
        return tonumber(num_str)
    end

    -- 解析乘除(优先级高于加减)
    local function parse_term()
        local value = parse_factor()
        if value == nil then
            return nil
        end
        while true do
            skip_spaces()
            local c = expr:sub(pos, pos)
            if c ~= "*" and c ~= "/" then
                return value
            end
            pos = pos + 1
            local rhs = parse_factor()
            if rhs == nil or (c == "/" and rhs == 0) then
                return nil
            end
            if c == "*" then
                value = value * rhs
            else
                value = value / rhs
            end
        end
    end

    -- 解析加减
    parse_expr = function()
        local value = parse_term()
        if value == nil then
            return nil
        end
        while true do
            skip_spaces()
            local c = expr:sub(pos, pos)
            if c ~= "+" and c ~= "-" then
                return value
            end
            pos = pos + 1
            local rhs = parse_term()
            if rhs == nil then
                return nil
            end
            if c == "+" then
                value = value + rhs
            else
                value = value - rhs
            end
        end
    end

    local result = parse_expr()
    -- 表达式必须被完整解析, 不允许有剩余字符
    skip_spaces()
    if result == nil or pos <= #expr then
        return nil
    end
    return result
end

-- 值中含数学运算符号(+ - * /)时尝试求值:
-- 成功返回结果的字符串形式(整数输出十进制, 非整数输出浮点), 失败返回 nil(调用方保留原值)
local function eval_math_value(value)
    if not value:find("[+%-*/]") then
        return nil
    end
    local result = eval_math_expression(value)
    if result == nil then
        return nil
    end
    local int_result = math.tointeger(result)
    if int_result then
        return string.format("%d", int_result)
    end
    return tostring(result)
end

-- 解析文件中的所有键值对(跳过空行与 # 注释行, 兼容 := 与 = 赋值)
-- 值中含数学运算符号(+ - * /)时先求值, 只保留计算结果(如 30*1024 -> 30720)
-- 返回 ordered_keys(键的顺序), key_value_pairs; 文件无法打开时返回 nil
local function parse_mk_to_key_value(input_filename)
    local file = io.open(input_filename, "r")
    if not file then
        return nil, "cannot open file: " .. input_filename
    end

    -- 用于存储键值对（有序）
    local ordered_keys = {}      -- 保存键的顺序
    local key_value_pairs = {}   -- 保存键值对

    for line in file:lines() do
        -- 去除行首尾的空格
        local trimmed_line = line:match("^%s*(.-)%s*$")

        -- 跳过空行与注释行
        if trimmed_line ~= "" and not trimmed_line:match("^#") then
            -- 先将 := 替换为 =
            local normalized_line = trimmed_line:gsub(":%s*=", "=")
            local key, value = normalized_line:match("^(.-)%s*=%s*(.+)$")
            if key and value then
                -- 去除键和值的首尾空格
                key = key:match("^%s*(.-)%s*$")
                value = value:match("^%s*(.-)%s*$")
	 -- 值中含数学运算符号时直接求值, 只保留计算结果
                local evaluated_value = eval_math_value(value)
                if evaluated_value then
                    value = evaluated_value
                end
                -- 只在第一次出现时记录键的顺序
                if key_value_pairs[key] == nil then
                    table.insert(ordered_keys, key)
                end
                key_value_pairs[key] = value
            end
        end
    end
    file:close()
    return ordered_keys, key_value_pairs
end

-- 把键值对按原顺序写入输出文件
local function write_key_value(output_filename, ordered_keys, key_value_pairs)
    local output_file = io.open(output_filename, "w")
    if not output_file then
        print("cannot create file: " .. output_filename)
        return false
    end
    for _, key in ipairs(ordered_keys) do
        output_file:write(key .. "=" .. key_value_pairs[key] .. "\n")
    end
    output_file:close()
    return true
end

local mode      = arg[1]  -- "check" 或 "sync"
local src_file  = arg[2]  -- user_alg_config.mk 路径
local snap_file = arg[3]  -- config.a 快照路径

if (mode ~= "check" and mode ~= "sync") or not src_file or not snap_file then
    print("usage: parse_user_alg_config_mk.lua check|sync <user_alg_config.mk> <config snapshot file>")
    os.exit(2)
end

-- 解析源配置文件
local src_keys, src_pairs = parse_mk_to_key_value(src_file)
if not src_keys then
    print("failed to parse source file: " .. src_file)
    os.exit(2)
end
if #src_keys == 0 then
    print("warning: no key-value pair found in " .. src_file)
    os.exit(2)
end

if mode == "check" then
    -- 比较快照与源文件中每个宏的值
    local snap_keys, snap_pairs = parse_mk_to_key_value(snap_file)
    local changed = false

    if not snap_keys then
        -- 快照不存在或无法读取, 视为需要重建
        changed = true
        print("snapshot file missing or unreadable: " .. snap_file)
    else
        for _, key in ipairs(src_keys) do
            if snap_pairs[key] ~= src_pairs[key] then
                changed = true
                print(string.format("config %s differs: snapshot[%s] now[%s]",
                                    key, tostring(snap_pairs[key]), tostring(src_pairs[key])))
                break
            end
        end
        if not changed then
            print(string.format("config identical to snapshot (%d items), no need to rebuild BNPU project", #src_keys))
        end
    end

    -- 0: 无变化; 1: 有变化(调用方应重建 BNPU 工程后再 sync)
    os.exit(changed and 1 or 0)
else
    -- sync: 把源文件所有宏按原顺序写入快照
    if not write_key_value(snap_file, src_keys, src_pairs) then
        os.exit(2)
    end
    print(string.format("%d macros written to snapshot: %s", #src_keys, snap_file))
    os.exit(0)
end
