#include "analyze.h"


using namespace std;





Analyze::Analyze()
{
	//Noting to do
	
}

Analyze::Analyze(const string& input, const string& output)
{
	readFile(input);
	write(output);
	
}

void Analyze::readFile(const string& input)
{
	ifstream fin(input.c_str(), ios::in);
	stringstream stream;
	stream << fin.rdbuf();
	fin.close();
	string st;
	
	 bool isData = false;
	 string toNode = "";
	 long int simulationTime = 0;
	
	//read one line from the stream
	while(getline(stream,st)){
		stringstream h;
		if(st.substr(0, 15) == "Simulation time"){
			h << st.substr(st.find_first_of(':')+1);
			h >> simulationTime;
		}
		if(st == "END-TO-END PERFORMANCE") {
			isData = true; //now the data part starts
			continue;
		}
		if(isData) { // start reading data
			if(st[0] == 'T' && st[1] == 'o') {
				toNode = st.substr(8);
				//cout << "* To node: " << toNode << endl;
			} else if (st[0] == '-') {
				string fromNode = "";
				int msgs = 0;
				int latency = 0;
				double throughput = 0.;
				string u; //unused part
				h << st.substr(7);
				h >> fromNode >> u >>  msgs >> u  >> latency >> u >> u >> throughput;
				//cout << "* data: " << fromNode << " " << msgs << " " << latency << " " << throughput << endl;
				addData(toNode, fromNode, simulationTime, msgs, latency, throughput);
			}
		}
		h.clear();
  }
}

void Analyze::addData(const string& toNode, const string& fromNode, int simTime, int msgs, int latency, double throughput)
{
	
	if(toNodeMap.find(toNode) != toNodeMap.end()) // toNode already exists
	{
		//cout << "toNode " << toNode << "already exists" << endl;
		fromNode_t* tmpFromNode = (toNodeMap.find(toNode)->second);
		if(tmpFromNode->find(fromNode) != tmpFromNode->end()) {
			//cout << "fromNode " << fromNode << "already exists" << endl;
			
			node* tmpNode = tmpFromNode->find(fromNode)->second;
			addData(tmpNode, simTime, msgs, latency, throughput);
			
		} else {
			//cout << "create fromNode " << fromNode << endl;
			
			node* tmpNode = new node;
			addData(tmpNode, simTime, msgs, latency, throughput);
			
			tmpFromNode->insert(pair<string, node*> (fromNode, tmpNode));
			
		}
	} else { //create new toNode
		//cout << "create toNode" << toNode << endl;
		node* tmpNode = new node;
		addData(tmpNode, simTime, msgs, latency, throughput);
		
		//cout << "create fromNode " << fromNode << endl;
		fromNode_t* tmpMap = new map<string, node*>;
		tmpMap->insert(pair<string, node*> (fromNode, tmpNode));
		
		toNodeMap.insert(pair<string, fromNode_t* > (toNode, tmpMap));
	}
}

void Analyze::addData(node* node, int simTime, int msgs, int latency, double throughput)
{
	node->time.push_back(simTime);
	node->msgs.push_back(msgs);
	node->latency.push_back(latency);
	node->throughput.push_back(throughput);
}

void Analyze::write(const string& output)
{
	
	toNode_t::iterator it;
	for(it=toNodeMap.begin(); it != toNodeMap.end(); it++) {
		//cout << it->first << " => " << endl;
		
		fromNode_t::iterator jt;
		for(jt=it->second->begin(); jt != it->second->end(); jt++ ) {
			
			fstream fstr;
			fstr.open((output + "_" + it->first + "-" + jt->first).c_str(), fstream::out);
			
			//cout << "\t" << jt->first << " => " <<  endl;
			
			list<int>::iterator time_it;
			list<int>::iterator msgs_it = jt->second->msgs.begin();
			list<int>::iterator latency_it = jt->second->latency.begin();
			list<double>::iterator throughput_it = jt->second->throughput.begin();
			
			fstr << "#to " << it->first << " from " << jt->first << endl;
			fstr << "time\t msgs\t latency\t throughput" << endl;
			for(time_it=jt->second->time.begin(); time_it !=jt->second->time.end(); time_it++) {
				fstr << *time_it << "\t" << *msgs_it++ << "\t" << *latency_it++ << "\t" << *throughput_it++ << endl;
			}
			
			fstr.close();
		}
	}
	cout << "Witing data files done." << endl;
	
	writeGnuplut(output);
}

/*create a gnuplot file*/
void Analyze::writeGnuplut(const string& output)
{
	fstream fstr;
	cout << "create file " << output << ".gnuplot" << endl;
	fstr.open((output + ".gnuplot").c_str(), fstream::out);
	
	fstr << "set xlabel \"time\"" << endl;
	fstr << "set ylabel \"msgs\"" << endl;

	fstr << "set title \"Messages\"" << endl;

	fstr << "set output '" << output << "-messages.png'" << endl;
	fstr << "set terminal png" << endl;
	
	fstr << "plot ";
	toNode_t::iterator it;
	bool isFirst = true;
	for(it=toNodeMap.begin(); it != toNodeMap.end(); it++) {
		fromNode_t::iterator jt;
		for(jt=it->second->begin(); jt != it->second->end(); jt++ ) {
			if(isFirst) {
				isFirst = false;
			} else {
				fstr << ",";
			}
			string file = output + "_" + it->first + "-" + jt->first;
			string title = "to " + it->first + " from " + jt->first;
			fstr << "'" << file << "' using 1:2 with linespoints title '" << title << "'";
		}
	}
	fstr << endl;
	
	fstr << "reset" << endl;
	
	fstr << "set xlabel \"time\"" << endl;
	fstr << "set ylabel \"msgs\"" << endl;
	
	fstr << "set title \"Latency\"" << endl;
	
	fstr << "set output '" << output << "-latency.png'" << endl;
	fstr << "set terminal png" << endl;
	
	fstr << "plot ";
	isFirst = true;
	for(it=toNodeMap.begin(); it != toNodeMap.end(); it++) {
		fromNode_t::iterator jt;
		for(jt=it->second->begin(); jt != it->second->end(); jt++ ) {
			if(isFirst) {
				isFirst = false;
			} else {
				fstr << ",";
			}
			string file = output + "_" + it->first + "-" + jt->first;
			string title = "to " + it->first + " from " + jt->first;
			fstr << "'" << file << "' using 1:3 with linespoints title '" << title << "'";
		}
	}
	fstr << endl;
	
	fstr << "reset" << endl;
	
	fstr << "set xlabel \"time\"" << endl;
	fstr << "set ylabel \"msgs\"" << endl;
	
	fstr << "set title \"Throughput\"" << endl;
	
	fstr << "set output '" << output << "-throughput.png'" << endl;
	fstr << "set terminal png" << endl;
	
	fstr << "plot ";
	isFirst = true;
	for(it=toNodeMap.begin(); it != toNodeMap.end(); it++) {
		fromNode_t::iterator jt;
		for(jt=it->second->begin(); jt != it->second->end(); jt++ ) {
			if(isFirst) {
				isFirst = false;
			} else {
				fstr << ",";
			}
			string file = output + "_" + it->first + "-" + jt->first;
			string title = "to " + it->first + " from " + jt->first;
			fstr << "'" << file << "' using 1:4 with linespoints title '" << title << "'";
		}
	}
	fstr << endl;
	
	fstr << "reset" << endl;
	fstr.close();
	
	cout << "Writing gnuplot file done." << endl;
}


int main(int argc, char** argv)
{
	if(argc >= 3) {
		Analyze a(argv[1], argv[2]);
	} else {
		std::cout << "usage: " << argv[0] << " <input> <output>" << std::endl;
	}
	return 0;
}