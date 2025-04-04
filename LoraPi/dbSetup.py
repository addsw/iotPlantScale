import sqlite3

connection_obj = sqlite3.connect('plant.db')

cursor_obj = connection_obj.cursor()

cursor_obj.execute("DROP TABLE IF EXISTS PLANT")

table = """ CREATE TABLE PLANT (
PlantID INT PRIMARY KEY,
Weight REAL NOT NULL,
Type VARCHAR(30) NOT NULL,
Address VARCHAR(255) NOT NULL,
Reserved BOOLEAN NOT NULL
);"""

cursor_obj.execute(table)

connection_obj.commit()

connection_obj.close()