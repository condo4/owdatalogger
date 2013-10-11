#!/usr/bin/python
import sys
import MySQLdb
import owdatalogger

config = owdatalogger.Config()
lstmeasure = config.ScanSensors()

try:
	dbconf = config['Database']
	con = MySQLdb.connect(dbconf['hostname'], dbconf['user'], dbconf['password'], dbconf['database']);
	cur = con.cursor()
	cur.execute("SELECT VERSION()")
	ver = cur.fetchone()
	print "Database version : %s " % ver
	cur = con.cursor()
	for meas in lstmeasure:
		try:
			meas.DbCreate()
			meas.Display()
		except MySQLdb.Error, e:
			print "Error %d: %s" % (e.args[0],e.args[1])
except MySQLdb.Error, e:
	print "Error %d: %s" % (e.args[0],e.args[1])
	sys.exit(1)
finally:
	if con:
		con.close()
