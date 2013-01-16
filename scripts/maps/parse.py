#!/usr/bin/python

def main():
  inspect()


def inspect():
  nodes = set()
  degree = {}
  tuples = parse()
  deg = []
  for edge in tuples:
    vertex = edge[0]

    nodes.add(vertex)

    if vertex not in degree: degree[vertex] = 0
    degree[vertex] = degree[vertex] + 1

  for (key, value) in degree.items():
    deg.append( value )

  deg.sort()
  print deg

def dot():
  tuples = parse()
  print 'graph G {'
  for edge in tuples:
    print edge[0], '--', edge[1], ';'
  print '}'


def parse(filename = 'map.20120101_nonStub'):
  with open(filename) as f:
    lines = f.readlines()
  
  parsed = map(lambda line: line.split(), lines)
  return parsed

main()
