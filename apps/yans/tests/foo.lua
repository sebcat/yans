b = yans.URLBuilder()
for k,v in pairs(b:parse("http://www.google.com/#")) do print(k,v) end
b = yans.URLBuilder(yans.URLFL_REMOVE_EMPTY_FRAGMENT)
for k,v in pairs(b:parse("http://www.google.com/#")) do print(k,v) end
