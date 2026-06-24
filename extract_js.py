import re

with open('web_serial_server.js', 'r', encoding='utf-8') as f:
    content = f.read()

m = re.search(r'function controlPageHtml\(\) \{.*?(<script>.*?</script>).*?\}', content, re.DOTALL | re.IGNORECASE)
if m:
    with open('frontend_js.js', 'w', encoding='utf-8') as f:
        f.write(m.group(1))
    print("Extracted script to frontend_js.js")
else:
    print("Could not find script block")
