
f = open('next')
lines = f.readlines()

new = map(lambda line: line.split(), lines)
new = map(lambda (src, dst): (int(src), int(src)), new)
new.sort()
print new 
