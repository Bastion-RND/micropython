include("$(MPY_DIR)/extmod/asyncio")
freeze("$(PORT_DIR)/modules")
freeze("modules")

# Useful networking-related packages.
require("bundle-networking")

# Require some micropython-lib modules.
# require("aioespnow")
require("umqtt.robust")
require("umqtt.simple")
require("upysh")

# require("logging")
