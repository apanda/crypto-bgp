
f = open('result')
lines = f.readlines()

lines = filter(lambda line: ('Next' in line) or ('SEVER' in line), lines)

l = []

counter = 0
for line in lines:
	if 'SEVER' in line: 
		l.append(counter)
		counter = 0
	if 'Next' in line: counter = counter + 1
	

print l
