#!/usr/bin/python
import ow
import owdatalogger
import os

config = owdatalogger.Config()

param = {}
if os.environ.has_key('QUERY_STRING'):
	for i in os.environ['QUERY_STRING'].replace('%2F','/').replace('%3A',':').split("&"):
		s = i.split("=")
		param[s[0]] = s[1]

print('Content-type: text/html')
print('')
print('<html>')
print('<head>')
print('<title>OW Data logger</title>')
print('</head>')
print('<body>')
print('<form>')
for sensor in config.ScanSensors():
	print('<h1>%s (%s)</h1>'%(sensor.GetType(),sensor.GetPath()))
	print('<table>')
	for meas in sensor:
		if param.has_key('submit'):
			if param.has_key(meas.id()) and param[meas.id()] == 'on':
				meas.setEnable(True)
			else:
				meas.setEnable(False)
		if meas.enable():
			checked = "checked='yes'"
		else:
			checked = ""
		print('<tr><td><input type="checkbox" name="%s" %s/></td><td>%s</td><td>%s</td></tr>'%(meas.id(),checked,meas.name.title(),meas.GetValue()))
	print('</table>')
print('<input type="submit" name="submit" value="Save">')
print('</form>')
if param.has_key('submit'):
	print('<h1>SAVED</h1>')
print('</body>')
print('</html>')
