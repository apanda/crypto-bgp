#!/usr/bin/python

from collections import deque

GRAPH_SIZE = 5976
BUCKET_SIZE = 2

buckets = deque()
for i in range(BUCKET_SIZE):
  buckets.append( [] )

def main():
  inspect()



def inspect():
  edges = parse()

  nodeMapping = {}
  degrees = {}
  for (src, dst, rel) in edges:
    vertex = src

    if vertex not in degrees: degrees[vertex] = 0
    degrees[vertex] = degrees[vertex] + 1



  degreeRanking = []
  for (vertex, degree) in degrees.items():
    degreeRanking.append( (degree, vertex) )

  degreeRanking.sort(key = lambda (degree, vertex): degree, reverse = True)

  for degree in degreeRanking:
    print degree

  counter = 0
  for (ranking, vertex) in degreeRanking:

    if counter > (GRAPH_SIZE):
      assert ranking == 1
      continue

    bucket = buckets.popleft()
    bucket.append(vertex)
    buckets.append(bucket)

    counter = counter + 1


  reverseMapping = {}
  bucketList = []
  for bucket in buckets:
    bucketList = bucketList + bucket

  counter = 0
  for vertex in bucketList:

    if counter > (GRAPH_SIZE):
      break

    if counter == 2210: print vertex
    reverseMapping[counter] = vertex
    nodeMapping[vertex] = counter
    counter = counter + 1

  '''
  lines = parse('sorted')
  for tokens in lines:
    vertex = int( tokens[4][:-1] )
    count = int( tokens[5] )
    print reverseMapping[vertex], count
  '''

  print 'graph G {'
  for i in range( GRAPH_SIZE ):
    nodeID = '[node_id=%d]' %(i)
    line = '%d %s;' %(i, nodeID)
    print line

  uniqueEdges = set()
  for (src, dst, rel) in edges:
    try:
      src = nodeMapping[src]
      dst = nodeMapping[dst]
    except: continue
    pair = [src, dst]
    pair.sort()
    uniqueEdges.add( tuple(pair) )

  for (src, dst) in uniqueEdges:
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
