#ifndef DMPGEXECUTORPUSH_H
#define DMPGEXECUTORPUSH_H

#include "../../../kukadu/include/kukadu.h"
#include "../../../kukadu/src/types/DMP.h"

#include <cstdio>
#include <iostream>
#include <fstream>
#include <thread>
#include <string>
#include <vector>
#include <wordexp.h>
#include "ros/ros.h"
#include "std_msgs/Int32.h"


using namespace std;
using namespace arma;

class DMPExecutorPush : public DMPExecutor {
private:
    string objectID;
    std::shared_ptr<SimInterface>  simI;
    int doSimulation;
    mat DesPos;
    vec ObjTimes;
    mat ObjCartPos;


public:
    DMPExecutorPush( std::shared_ptr<Dmp> dmp, std::shared_ptr<ControlQueue> execQueue, string env, std::shared_ptr<SimInterface> simI,string objectID);
    double addTerm(double t, const double* currentDesiredYs, int jointNumber, std::shared_ptr<ControlQueue> queue) override ;
    void setObjectData(arma::vec times, arma::mat cartPos);
};



#endif // DMPGEXECUTORPUSH_H