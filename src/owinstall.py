#!/usr/bin/python
import ow
import sys
import MySQLdb

config = {}

class Measure(object):
	def __init__(self,name,path,attr,ratio):
		self.name = name
		self.path = path
		self.attr = attr
		self.ratio = ratio

	def getquery(self):
		return "INSERT INTO sensors (name,path,ratio,delay) VALUES (\"%s\",\"%s/%s\",%i,%i);"%(self.name,self.path,self.attr,self.ratio,15)

conf = open("/etc/owdatalogger.cfg")
for line in conf.readlines():
	l = line.strip()
	if len(l) > 0 and l[0] == '[':
		secname = l[1:-1]
		config[secname] = {}
	else:
		c = l.split("=")
		if len(c) == 2:
			config[secname][c[0].strip()] = c[1].strip()

ow.init( config['OWFS']['port'] )
root = ow.Sensor("/")
lstmeasure = []
for sensor in root.sensorList():
	attrs = []
	if 'MultiSensor/' in  sensor._attrs.keys():
		mlttype = ow.owfs_get('%s/MultiSensor/type'%sensor._path)
		print 'Multi Sensor %s'%mlttype
		if mlttype == 'MS-TH':
			lstmeasure.append(Measure("temperature",sensor._path,"temperature",1))
			lstmeasure.append(Measure("humidity",sensor._path,"HIH4000/humidity",1))
		elif mlttype == 'MS-TL':
			lstmeasure.append(Measure("temperature",sensor._path,"temperature",1))
			lstmeasure.append(Measure("humidity",sensor._path,"VAD",20))
		else:
			print "Unknown multi Sensor: %s"%mlttype
	else:
		print "TODO: manage other sensor"

try:
	dbconf = config['Database']
	con = MySQLdb.connect(dbconf['hostname'], dbconf['user'], dbconf['password'], dbconf['database']);
	cur = con.cursor()
	cur.execute("SELECT VERSION()")
	ver = cur.fetchone()
	print "Database version : %s " % ver
	cur = con.cursor()
	for meas in lstmeasure:
		cur.execute(meas.getquery())
except MySQLdb.Error, e:
	print "Error %d: %s" % (e.args[0],e.args[1])
	sys.exit(1)
finally:
	if con:
		con.close()
