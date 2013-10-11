import ow
import MySQLdb

configuration = None

class OwSensorMeasure(object):
	def __init__(self,parent, name,attr,ratio=1):
		self.name = name
		self.attr = attr
		self.ratio = ratio
		self._parent = parent

	def GetValue(self):
		return float(ow.owfs_get('%s/%s'%(self._parent.GetPath(),self.attr))) * self.ratio
	
	def id(self):
		return '%s:%s'%(self._parent.GetPath(),self.attr)
	
	def DbCreate(self):
		self._parent.DbCreate()
		cfg = Config()
		cur = cfg.cursor()
		cur.execute("SELECT count(*) FROM sensors, sensor_entry WHERE sensor_entry.path='%s' AND attr='%s'"%(self._parent.GetPath(),self.attr))
		exist = cur.fetchone()
		if exist[0] == 0:
			cur.execute("INSERT INTO sensor_entry(path,attr,ratio,delay) VALUES('%s','%s','%s','%s')"%(self._parent.GetPath(),self.attr, self.ratio,15))
	
	def enable(self):
		cfg = Config()
		cur = cfg.cursor()
		cur.execute("SELECT count(*) FROM sensors, sensor_entry WHERE sensor_entry.path='%s' AND attr='%s'"%(self._parent.GetPath(),self.attr))
		exist = cur.fetchone()
		if exist[0] == 0:
			return False
		cur.execute("SELECT enable FROM sensors, sensor_entry WHERE sensor_entry.path='%s' AND attr='%s'"%(self._parent.GetPath(),self.attr))
		enable = cur.fetchone()
		if enable[0] == 1:
			return True
		return False
	
	def setEnable(self,value):
		self.DbCreate()
		cfg = Config()
		cur = cfg.cursor()
		if value:
			cur.execute("UPDATE sensor_entry SET enable = 1 WHERE sensor_entry.path='%s' AND attr='%s'"%(self._parent.GetPath(),self.attr))
		else:
			cur.execute("UPDATE sensor_entry SET enable = 0 WHERE sensor_entry.path='%s' AND attr='%s'"%(self._parent.GetPath(),self.attr))


class OwSensor(list):
	def __init__(self,owobj):
		self._owobj = owobj
		self._subtype = None
		
		if self._owobj.type == 'DS2438':
			if 'MultiSensor/' in  self._owobj._attrs.keys():
				self._subtype = ow.owfs_get('%s/MultiSensor/type'%self._owobj._path)
				if self._subtype == 'MS-TH':
					self.append(OwSensorMeasure(self,"temperature","temperature"))
					self.append(OwSensorMeasure(self,"humidity","HIH4000/humidity"))
				elif self._subtype == 'MS-TL':
					self.append(OwSensorMeasure(self,"temperature","temperature"))
					self.append(OwSensorMeasure(self,"light","VAD",20))
				else:
					print "Unknown multi Sensor: %s"%mlttype
		elif self._owobj.type == 'DS18B20':
			self.append(OwSensorMeasure(self,"temperature","temperature"))
		else:
			print "TODO: manage other sensor:\n\tFamily: "+self._owobj.family+"\n\tType: "+self._owobj.type
	
	def GetType(self):
		if self._subtype != None:
			return '%s (%s)'%(self._subtype,self._owobj.type)
		return self._owobj.type
	
	def GetPath(self):
		return self._owobj._path
	
	def AddMeasure(self,name,attr,ratio=1):
		self.append(OwSensorMeasure(name,attr,ratio))
	
	def Display(self):
		print("Sensor %s"%self._owobj._path)
		for m in self:
			print("\tMeasure: %s %s %s"%(m.name,m.attr,m.GetValue()))
	
	def DbCreate(self):
		cfg = Config()
		cur = cfg.cursor()
		cur.execute("SELECT count(*) FROM sensors WHERE path='%s'"%self.GetPath())
		exist = cur.fetchone()
		if exist[0] == 0:
			if self._subtype == None:
				cur.execute("INSERT INTO sensors(path,type) VALUES('%s','%s')"%(self.GetPath(),self._owobj.type))
			else:
				cur.execute("INSERT INTO sensors(path,type,subtype) VALUES('%s','%s','%s')"%(self.GetPath(),self._owobj.type,self._subtype))


class Config(dict):
	_instance = None
	
	def __new__(cls, *args, **kwargs):
		if not cls._instance:
			cls._instance = super(Config, cls).__new__(cls, *args, **kwargs)
		return cls._instance
	
	def __init__(self):
		conf = open("/etc/owdatalogger.cfg")
		for line in conf.readlines():
			l = line.strip()
			if len(l) > 0 and l[0] == '[':
				secname = l[1:-1]
				self[secname] = {}
			else:
				c = l.split("=")
				if len(c) == 2:
					self[secname][c[0].strip()] = c[1].strip()
		conf.close()
		self._init = False
		self._db = None
	
	def init(self):
		if self._init == True:
			return
		ow.init( self['OWFS']['port'] )
		self._init = True

	def ScanSensors(self):
		self.init()
		root = ow.Sensor("/")
		lstSensors = []
		for sensor in root.sensorList():
			lstSensors.append(OwSensor(sensor))
		return lstSensors

	def cursor(self):
		if self._db == None:
			dbconf = self['Database']
			self._db = MySQLdb.connect(dbconf['hostname'], dbconf['user'], dbconf['password'], dbconf['database']);
		return self._db.cursor()


