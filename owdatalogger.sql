DROP DATABASE IF EXISTS weather;
CREATE DATABASE weather;
USE weather;

CREATE TABLE sensors (
	path CHAR(64) NOT NULL,
	delay MEDIUMINT,
	PRIMARY KEY (path)
);

CREATE TABLE measures (
	id BIGINT NOT NULL AUTO_INCREMENT,
	date TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP, 
	path CHAR(64) NOT NULL,
	value FLOAT NOT NULL,
	PRIMARY KEY (id)
);

INSERT INTO sensors (path,delay) VALUES ("/26.8046F5000000/temperature",15);
INSERT INTO sensors (path,delay) VALUES ("/26.8046F5000000/humidity",15);
INSERT INTO sensors (path,delay) VALUES ("/26.AF7486000000/temperature",15);
INSERT INTO sensors (path,delay) VALUES ("/26.AF7486000000/humidity",15);

GRANT ALL PRIVILEGES ON weather TO  'weather'@'localhost' IDENTIFIED BY 'weather43' WITH GRANT OPTION;
