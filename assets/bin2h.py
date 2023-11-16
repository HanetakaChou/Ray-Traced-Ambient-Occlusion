import sys

if "__main__" == __name__:
	if(3 == len(sys.argv)):
		try:
			with open(sys.argv[1], 'rb') as input_file:
				binary_data = input_file.read()
			with open(sys.argv[2], 'w') as output_file:
				output_file.write(", ".join([f"0X{ubyte:02X}U" for ubyte in binary_data]))
		except:
			sys.exit(1)
	else:
		sys.exit(1)
