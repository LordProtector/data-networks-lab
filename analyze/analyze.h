#ifndef ANALYZE_H
#define ANALYZE_H

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstring>
#include <vector>
#include <map>
#include <list>
#include <utility>
#include <sys/types.h>


struct node {
	std::list<int64_t> time;
	std::list<int> msgs;
	std::list<int> latency;
	std::list<double> throughput;
};

typedef std::map<std::string, node*> fromNode_t;
typedef std::map<std::string, fromNode_t*> toNode_t;


class Analyze
{
public:
	Analyze();
	
	Analyze(const std::string& input, const std::string& output);
	
	
private:
	
	void readFile(const std::string& file);
	
	void writeGnuplut(const std::string& output);
	
	void write(const std::string& file);
	
	void addData(const std::string& toNode, const std::string& fromNode, int64_t simTime, int msgs, int latency, double throughput);
	
	void addData(node* node, int64_t simTime, int msgs, int latency, double throughput);
	
	//to node from node
	toNode_t toNodeMap;
};

#endif