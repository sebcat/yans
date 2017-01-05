#!/usr/bin/env lua
-- vim: shiftwidth=2 tabstop=2 expandtab

Addr = yans.Addr
Block = yans.Block

local function test_addr_tostring_self(str)
	addr = Addr(str)
	if tostring(addr) ~= str then
    error(tostring(addr).." ~= "..str)
  end
end

-- __tostring
test_addr_tostring_self("127.0.0.1")
test_addr_tostring_self("ff02::1")

-- __eq
assert(Addr"127.0.0.1" == Addr"127.0.0.1")
assert(Addr"127.0.0.1 127.0.6.4" == Addr"127.0.0.1")
assert(Addr"\t 127.0.0.1\t " == Addr"127.0.0.1")
assert(Addr"127.0.0.1" ~= Addr"127.0.0.0")
assert(Addr"127.0.0.1" ~= Addr"127.0.0.2")
assert(Addr"ff02::1" == Addr"ff02::1")
assert(Addr"ff02::1" ~= Addr"ff02::")
assert(Addr"ff02::1" ~= Addr"ff02::2")

-- __lt
assert(Addr"127.0.0.0" < Addr"127.0.0.1")
assert(not(Addr"127.0.0.1" < Addr"127.0.0.1"))
assert(not(Addr"127.0.0.2" < Addr"127.0.0.1"))
assert(Addr"ff02::1" < Addr"ff02::2")
assert(not(Addr"ff02::1" < Addr"ff02::1"))
assert(not(Addr"ff02::2" < Addr"ff02::1"))

-- __le
assert(Addr"127.0.0.0" <= Addr"127.0.0.1")
assert(Addr"127.0.0.1" <= Addr"127.0.0.1")
assert(not(Addr"127.0.0.2" <= Addr"127.0.0.1"))
assert(Addr"ff02::1" <= Addr"ff02::2")
assert(Addr"ff02::1" <= Addr"ff02::1")
assert(not(Addr"ff02::2" <= Addr"ff02::1"))

-- __add
local a = Addr"126.255.255.255"
local res = a + 2
assert(a == Addr"126.255.255.255")
assert(res == Addr"127.0.0.1")
local a = Addr"ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff"
local res = a + 2
assert(a == Addr"ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff")
assert(res == Addr"::1")


-- __sub
local a = Addr"127.0.0.1"
local res = a - 2
assert(a == Addr"127.0.0.1")
assert(res == Addr"126.255.255.255")
local a = Addr"::1"
local res = a - 2
assert(a == Addr"::1")
assert(res == Addr"ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff")

-- mutable add method
local function addr_add(addr, n, expected)
  actual = Addr(addr):add(n)
  if tostring(actual) ~= expected then
    error(addr.." + "..tostring(n)..": expected "..expected..
        ", was: "..tostring(actual))
  end
end
local a = Addr"127.0.0.1"
a:add(1)
assert(a == Addr"127.0.0.2")
addr_add("255.255.255.255", 1, "0.0.0.0")
addr_add("0.0.255.255", 1, "0.1.0.0")
addr_add("0.0.0.0", -1, "255.255.255.255")
addr_add("0.1.0.0", -1, "0.0.255.255")
addr_add("127.0.0.1", -1, "127.0.0.0")
addr_add("127.0.0.0", 1, "127.0.0.1")
addr_add("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff", 1, "::")
addr_add("::ffff:ffff:ffff:ffff", 1, "0:0:0:1::")
addr_add("::", -1, "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff")
addr_add("::1:0:0:0:0", -1, "::ffff:ffff:ffff:ffff")
addr_add("ff02::1", 1, "ff02::2")
addr_add("::1", -1, "::")

-- mutable sub method
local function addr_sub(addr, n, expected)
  actual = Addr(addr):sub(n)
  if tostring(actual) ~= expected then
    error(addr.." + "..tostring(n)..": expected "..expected..", was: "
        ..tostring(actual))
  end
end
local a = Addr"127.0.0.1"
a:sub(1)
assert(a == Addr"127.0.0.0")
addr_sub("255.255.255.255", -1, "0.0.0.0")
addr_sub("0.0.255.255", -1, "0.1.0.0")
addr_sub("0.0.0.0", 1, "255.255.255.255")
addr_sub("0.1.0.0", 1, "0.0.255.255")
addr_sub("127.0.0.1", 1, "127.0.0.0")
addr_sub("127.0.0.0", -1, "127.0.0.1")
addr_sub("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff", -1, "::")
addr_sub("::ffff:ffff:ffff:ffff", -1, "0:0:0:1::")
addr_sub("::", 1, "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff")
addr_sub("::1:0:0:0:0", 1, "::ffff:ffff:ffff:ffff")
addr_sub("ff02::1", -1, "ff02::2")
addr_sub("::1", 1, "::")

-- yans.Block first, last
local function block(s, efirst, elast)
  blk = Block(s)
  afirst, alast = tostring(blk:first()), tostring(blk:last())
  if afirst ~= efirst or alast ~= elast then
    error(string.format("expected %s-%s, was %s-%s",
        efirst, elast, afirst, alast))
  end
end
block("::/65", "::", "::7fff:ffff:ffff:ffff")
block("::/0", "::", "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff")
block("::/128", "::", "::")
block("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff/65",
    "ffff:ffff:ffff:ffff:8000::", "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff")
block("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff/0",
    "::", "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff")
block("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff/128",
    "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff",
    "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff")
block("2606:2800:220:1:248:1893:25c8:1946/112",
    "2606:2800:220:1:248:1893:25c8:0",
    "2606:2800:220:1:248:1893:25c8:ffff")
block("0.0.0.0/23", "0.0.0.0", "0.0.1.255")
block("0.0.0.0/0", "0.0.0.0", "255.255.255.255")
block("0.0.0.0/32", "0.0.0.0", "0.0.0.0")
block("255.255.255.255/23", "255.255.254.0", "255.255.255.255")
block("255.255.255.255/0", "0.0.0.0", "255.255.255.255")
block("255.255.255.255/32", "255.255.255.255", "255.255.255.255")

local function block_str(s, expected)
  blk = Block(s)
  actual = tostring(blk)
  if actual ~= expected then
    error(string.format("expected %q, got %q", expected, actual))
  end
end
block_str("::/65", "::-::7fff:ffff:ffff:ffff")
block_str("::/0", "::-ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff")
block_str("::/128", "::-::")
block_str("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff/65",
    "ffff:ffff:ffff:ffff:8000::-ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff")
block_str("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff/0",
    "::-ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff")
block_str("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff/128",
    "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff-"..
    "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff")
block_str("2606:2800:220:1:248:1893:25c8:1946/112",
    "2606:2800:220:1:248:1893:25c8:0-"..
    "2606:2800:220:1:248:1893:25c8:ffff")
block_str("0.0.0.0/23", "0.0.0.0-0.0.1.255")
block_str("0.0.0.0/0", "0.0.0.0-255.255.255.255")
block_str("0.0.0.0/32", "0.0.0.0-0.0.0.0")
block_str("255.255.255.255/23", "255.255.254.0-255.255.255.255")
block_str("255.255.255.255/0", "0.0.0.0-255.255.255.255")
block_str("255.255.255.255/32", "255.255.255.255-255.255.255.255")

local function block_err(s)
  local ok, res = pcall(Block, s)
  if ok then
    error("expected failure, got "..tostring(res))
  end
end
block_err("")
block_err("garbage")
block_err("127.0.0.1-garbage")
block_err("garbage-127.0.0.1")
block_err("gar-bage")
block_err("127.0.0.1/e")
block_err("garbage/24")
block_err("garbage/e")
block_err("127.0.0.1-::1")
block_err("::1-127.0.0.1")

local function block_contains(blk, addr, expected_in)
  blk = Block(blk)
  addr = Addr(addr)
  if blk:contains(addr) ~= expected_in then
    if expected_in then
      error(string.format("%s not in %s", addr, blk))
    else
      error(string.format("%s contains %s", addr, blk))
    end
  end
end

block_contains("127.0.0.0/25", "126.255.255.255", false)
block_contains("127.0.0.0/25", "127.0.0.0", true)
block_contains("127.0.0.0/25", "127.0.0.64", true)
block_contains("127.0.0.0/25", "127.0.0.127", true)
block_contains("127.0.0.0/25", "127.0.0.128", false)
block_contains("127.0.0.1/32", "127.0.0.0", false)
block_contains("127.0.0.1/32", "127.0.0.1", true)
block_contains("127.0.0.1/32", "127.0.0.2", false)

block_contains("2001::1/127",
  "2000:ffff:ffff:ffff:ffff:ffff:ffff:ffff", false)
block_contains("2001::1/127", "2001::", true)
block_contains("2001::1/127", "2001::1", true)
block_contains("2001::1/127", "2001::2", false)
block_contains("2001::1/128", "2001::0", false)
block_contains("2001::1/128", "2001::1", true)
block_contains("2001::1/128", "2001::2", false)

-- assume we have network interfaces
assert(type(yans.devices()) == "table")

-- print some network information
do
  for _, dev in pairs(yans.devices()) do
    print(string.format("interface: %s",dev.name))
    if dev.loopback then print("loopback") end
    if dev.up then print("up") end
    if dev.running then print("running") end
    if dev.addresses ~= nil then
      print("addresses:")
      for _, addr in pairs(dev.addresses) do
        if addr.addr then
          print(string.format("    addr: %s", addr.addr))
        end
        if addr.netmask then
          print(string.format("    mask: %s", addr.netmask))
        end
        if addr.block then
          print(string.format("   range: %s", addr.block))
        end
        if addr.broadaddr then
          print(string.format("   bcast: %s", addr.broadaddr))
        end
        if addr.dstaddr then
          print(string.format(" dstaddr: %s", addr.dstaddr))
        end
        print("")
      end
    end
  end
end

local function get_loopback_ifname()
  for _, dev in pairs(yans.devices()) do
    if dev.loopback then return dev.name end
  end
end

-- Test IPv6 zone ID
-- assume we have a standard IPv6 loopback setup with support for
-- broadcast address(es)
test_addr_tostring_self(string.format("ff02::1%%%s", get_loopback_ifname()))
test_addr_tostring_self(string.format("ff02::2%%%s", get_loopback_ifname()))
