
function test_url_resolve(builder, base, ref, expected)
  local actual = builder:resolve(base, ref)
  if actual ~= expected then
    error(string.format("resolve: expected:%q actual:%q", expected, actual))
  end
end
builder = yans.URLBuilder(yans.URLFL_REMOVE_EMPTY_QUERY |
    yans.URLFL_REMOVE_EMPTY_FRAGMENT)
test_url_resolve(builder,"http://example.com/", "http://www.example.aom",
    "http://www.example.aom/")
