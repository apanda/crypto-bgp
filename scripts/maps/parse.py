#!/usr/bin/python

def main():
  inspect()



def inspect():
  edges = parse()

  nodeMapping = {}
  degree = {}
  for edge in edges:
    vertex = edge[0]

    if vertex not in degree: degree[vertex] = 0
    degree[vertex] = degree[vertex] + 1

  degreeRanking = []
  for (key, value) in degree.items():
    degreeRanking.append( (value, key) )

  degreeRanking.sort(key = lambda x: x[0], reverse = True)

  counter = 0
  for ranking in degreeRanking:
    vertex = ranking[1]
    nodeMapping[vertex] = counter
    counter = counter + 1

  print 'graph G {'
  for i in range(counter):
    nodeID = '[node_id=%d]' %(i)
    line = '%d %s;' %(i, nodeID)
    print line

  uniqueEdges = set()
  for edge in edges:
    src = edge[0]
    dst = edge[1]
    src = nodeMapping[src]
    dst = nodeMapping[dst]
    pair = [src, dst]
    pair.sort()
    uniqueEdges.add( tuple(pair) )

  for edge in uniqueEdges:
    src = edge[0]
    dst = edge[1]
    line = '%s -- %s;' %(src, dst)
    print line

  print '}'

def dot():
  edges = parse()
  print 'graph G {'
  for edge in edges:
    print edge[0], '--', edge[1], ';'
  print '}'



def parse(filename = 'map.20120101_nonStub'):
  with open(filename) as f:
    lines = f.readlines()
  
  parsed = map(lambda line: line.split(), lines)
  return parsed

main()
