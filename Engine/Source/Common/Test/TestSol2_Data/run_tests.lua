
require "bootstrap-tests"
require "size"
require "type"
require "gc"
require "channel"
require "thread"
require "thread-interrupt"
require "shared-table"
require "metatable"
require "type_mismatch"
require "upvalues"
require "dump_table"
require "function"

if os.getenv("STRESS") then
 require "channel-stress"
 require "thread-stress"
 require "gc-stress"
end

test.summary()