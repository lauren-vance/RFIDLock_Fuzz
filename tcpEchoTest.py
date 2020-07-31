import socket
import threading
import time 
import signal 
import sys 
def signal_handler(signal, frame): 
	print ('You pressed Ctrl+C!' )
	s.close() 
	sys.exit(0) 

signal.signal(signal.SIGINT, signal_handler) 

ECHO_SERVER_ADDRESS = "192.168.1.140" 
ECHO_PORT = 7 
message = 'Hello, World!'
prompt = '\nPlease input a request from the following:\n--------------------------------------------\n\'A\' : Check Total Number of Cards Detected\n\'B\' : Check Total Number of Unlocks\n\'C\' : Check Total Number of Accesses Denied\n\'D\' : Display List of Currently Stored Keys\n\'E\' : Add New RFID Card Key\n\'F\' : Delete a Stored RFID Key\n\'Q\' : Quit Program\n--------------------------------------------\n' 
keyList = []

def pollLock():
	while True:
		sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM) 
		sock.connect((ECHO_SERVER_ADDRESS, ECHO_PORT)) 
		sock.settimeout(1000)
		request = 'd.'
		sock.sendall(request.encode())
		data = str()
		flag = 1
		while flag:
			recvChar = sock.recv(1)
			recvChar = recvChar.decode()
			data += recvChar
			if data[-1] == '.':
				flag = 0
		sock.close()
		listCheck = list(data.split(" "))
		for elem in reversed(listCheck):
			if not elem.isdigit():
				listCheck.remove(elem)
		# listCheck = what was returned from lock
		# if listCheck doesn't match keyList (stored here), send keyList to lock program

		if listCheck != keyList:
			print('[Mismatch found, sending list of keys to lock...]')
			sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM) 
			sock.connect((ECHO_SERVER_ADDRESS, ECHO_PORT)) 
			sock.settimeout(1000)
			request = 'List of Keys:' + ','.join(keyList) + '.'
			sock.sendall(request.encode())
			data = str()
			data = sock.recv(1024)
			data = data.decode()
			sock.close()
			print(data)
		# wait 20 seconds, then repeat
		time.sleep(20)
	

print('\n=== Lock Program - Client Portal ===\n')


# get current keyList from lock at startup/reset --
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM) 
s.connect((ECHO_SERVER_ADDRESS, ECHO_PORT)) 
s.settimeout(1000)
request = 'd.'
s.sendall(request.encode())
data = str()
flag = 1
while flag:
	recvChar = s.recv(1)
	recvChar = recvChar.decode()
	data += recvChar
	if data[-1] == '.':
		flag = 0
keyList = list(data.split(" "))
for elem in reversed(keyList):
	if not elem.isdigit():
		keyList.remove(elem)
print('\n===== Current List of Keys =====')
for elem in keyList:
	print(elem)
s.close()

# start thread function --
t = threading.Thread(target = pollLock, args = [], daemon = True)
t.start()

while 1:	

	# waiting for request from user to send to lock --

	request = input(prompt)
	print('You entered ' + request)
	if request == 'Q' or request == 'q':
		break
	if request == 'f' or request == 'F':
		print('Please select a key to delete:')
		print(keyList)
		dltKey = input()
		request += dltKey

	request = request + '.'
	s = socket.socket(socket.AF_INET, socket.SOCK_STREAM) 
	s.connect((ECHO_SERVER_ADDRESS, ECHO_PORT)) 
	s.settimeout(1000)
	print ('Sending request: ' + request)
	s.sendall(request.encode())
	data = str()
	flag = 1
	while flag:
		recvChar = s.recv(1)
		recvChar = recvChar.decode()
		data += recvChar
		if data[-1] == '.':
			flag = 0
	print ('Received: "' + data + '," ' + str(len(data)) + ' total bytes.')
	if request == 'd.' or request == 'D.':
		keyList = list(data.split(" "))
		for elem in reversed(keyList):
			if not elem.isdigit():
				keyList.remove(elem)
		print('\n===== Current List of Keys =====')
		for elem in keyList:
			print(elem)
		
	if request == 'e.' or request == 'E.' or "f" in request or "F" in request:
		if "Successful" in data:
			keyList = list(data.split(" "))
			for elem in reversed(keyList):
				if not elem.isdigit():
					keyList.remove(elem)
			print('\n===== Updated List of Keys =====')
			for elem in keyList:
				print(elem)

s.close() 
print('\nGoodbye!\n')