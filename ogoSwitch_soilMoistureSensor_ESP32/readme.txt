Ref. https://github.com/nrwiersma/ConfigManager

set configuration
curl -v -H 'Content-Type: application/json' -X PUT -d @config.json http://192.168.2.68/settings

read configuration
curl -v http://192.168.2.68/settings
