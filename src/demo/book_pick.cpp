#include "../utils/easyloggingpp/src/easylogging++.h"
_INITIALIZE_EASYLOGGINGPP

#include <iostream>
#include <string>
#include <armadillo>
#include <thread>
#include <vector>
#include <boost/program_options.hpp>

#include "../../include/kukadu.h"


#define ROBOT_TYPE "real"
#define ROBOT_SIDE "left"

#define CONTROL_RIGHT false

using namespace std;
using namespace arma;
namespace po = boost::program_options;

int main(int argc, char** args) {

    int importanceSamplingCount;
    double tau, az, bz, dmpStepSize, tolAbsErr, tolRelErr, ac, as, alpham;
    string inDir, cfFile, dataFolder, trajFile;
    vector<double> rlExploreSigmas;

    // Declare the supported options.
    po::options_description desc("Allowed options");
    desc.add_options()
            ("metric.database", po::value<string>(), "database folder")
            ("metric.goldStandard", po::value<string>(), "gold standard file")
            ("metric.alpham", po::value<double>(), "alpham")
            ("metric.exploreSigmas", po::value<string>(), "reinforcement learning exploration sigmas")
            ("metric.importanceSamplingCount", po::value<int>(), "size of importance sampling vector")
            ("metric.as", po::value<double>(), "as")
            ("dmp.tau", po::value<double>(), "tau")
            ("dmp.az", po::value<double>(), "az")
            ("dmp.bz", po::value<double>(), "bz")
            ("dmp.dmpStepSize", po::value<double>(), "dmp time step size")
            ("dmp.tolAbsErr", po::value<double>(), "tolerated absolute error")
            ("dmp.tolRelErr", po::value<double>(), "tolerated relative error")
            ("dmp.ac", po::value<double>(), "ac")
            ("mes.folder", po::value<string>(), "measurment data folder")
            ("pick.trajectory", po::value<string>(), "data for measured trajectory")
    ;

    ifstream parseFile(resolvePath("$KUKADU_HOME/cfg/book_pick.prop"), std::ifstream::in);
    po::variables_map vm;
    po::store(po::parse_config_file(parseFile, desc), vm);
    po::notify(vm);

    if (vm.count("dmp.tau")) tau = vm["dmp.tau"].as<double>();
    else return 1;
    if (vm.count("dmp.az")) az = vm["dmp.az"].as<double>();
    else return 1;
    if (vm.count("dmp.bz")) bz = vm["dmp.bz"].as<double>();
    else return 1;
    if (vm.count("dmp.dmpStepSize")) dmpStepSize = vm["dmp.dmpStepSize"].as<double>();
    else return 1;
    if (vm.count("dmp.tolAbsErr")) tolAbsErr = vm["dmp.tolAbsErr"].as<double>();
    else return 1;
    if (vm.count("dmp.tolRelErr")) tolRelErr = vm["dmp.tolRelErr"].as<double>();
    else return 1;
    if (vm.count("dmp.ac")) ac = vm["dmp.ac"].as<double>();
    else return 1;

    if (vm.count("metric.as")) as = vm["metric.as"].as<double>();
    else return 1;
    if (vm.count("metric.database")) inDir = resolvePath(vm["metric.database"].as<string>());
    else return 1;
    if (vm.count("metric.goldStandard")) cfFile = resolvePath(vm["metric.goldStandard"].as<string>());
    else return 1;
    if (vm.count("metric.alpham")) alpham = vm["metric.alpham"].as<double>();
    else return 1;
    if (vm.count("metric.exploreSigmas")) {
        string tokens = vm["metric.exploreSigmas"].as<string>();
        stringstream parseStream(tokens);
        char bracket;
        double dToken;
        parseStream >> bracket;
        while(bracket != '}') {
            parseStream >> dToken >> bracket;
            rlExploreSigmas.push_back(dToken);
        }
    } else return 1;
    if (vm.count("metric.importanceSamplingCount")) importanceSamplingCount = vm["metric.importanceSamplingCount"].as<int>();
    else return 1;

    if (vm.count("mes.folder")) dataFolder = resolvePath(vm["mes.folder"].as<string>());
    else return 1;

    if (vm.count("pick.trajectory")) trajFile = resolvePath(vm["pick.trajectory"].as<string>());
    else return 1;

    cout << "all properties loaded" << endl;
    int kukaStepWaitTime = dmpStepSize * 1e6;

    ros::init(argc, args, "kukadu"); ros::NodeHandle* node = new ros::NodeHandle(); usleep(1e6);

    shared_ptr<RosSchunk> leftHand = shared_ptr<RosSchunk>(new RosSchunk(*node, ROBOT_TYPE, ROBOT_SIDE));
    vector<double> leftHandJoints = {0, -0.38705404571511043, 0.7258474179992682, 0.010410616072391092, -1.2259735578027993, -0.4303327436948519, 0.8185967300722126};
    leftHand->publishSdhJoints(leftHandJoints);

    if(CONTROL_RIGHT) {

        shared_ptr<RosSchunk> rightHand = shared_ptr<RosSchunk>(new RosSchunk(*node, ROBOT_TYPE, "right"));
        vector<double> rightHandJoints = {0, -0.5231897140548627, -0.09207113810135568, 0.865287869514727, 1.5399390781924753, -0.6258846012187079, 0.01877915351042305};
        rightHand->publishSdhJoints(rightHandJoints);

    }

    shared_ptr<OrocosControlQueue> leftQueue = shared_ptr<OrocosControlQueue>(new OrocosControlQueue(argc, args, kukaStepWaitTime, ROBOT_TYPE, ROBOT_SIDE + string("_arm"), *node));
    shared_ptr<thread> lqThread = leftQueue->startQueueThread();

    shared_ptr<OrocosControlQueue> rightQueue = nullptr;
    shared_ptr<thread> rqThread = nullptr;

    if(CONTROL_RIGHT) {

        rightQueue = shared_ptr<OrocosControlQueue>(new OrocosControlQueue(argc, args, kukaStepWaitTime, ROBOT_TYPE, "right_arm", *node));
        rqThread = rightQueue->startQueueThread();

    }

    usleep(1e6);

    if(CONTROL_RIGHT) {

        rightQueue->switchMode(OrocosControlQueue::KUKA_JNT_IMP_MODE);
        rightQueue->moveJoints(stdToArmadilloVec({-2.0805978775024414, 1.4412447214126587, -2.203258752822876, 1.7961543798446655, 2.526369333267212, -1.6385622024536133, -0.7103539705276489}));

    }

    // measured joints were -0.2252   1.3174  -2.1671   0.4912   0.8510  -1.5699   1.0577
    mes_result currentJoints = leftQueue->getCurrentJoints();
    cout << currentJoints.joints.t() << endl;

//    leftQueue->setStiffness(0.2, 0.01, 0.2, 15000, 150, 2.0);
    leftQueue->switchMode(OrocosControlQueue::KUKA_JNT_IMP_MODE);
    leftQueue->moveJoints(stdToArmadilloVec({-0.514099, 1.83194, 1.95971, -0.99676, -0.0903862, 0.987185, 1.16542}));

    cout << "(main) press key to continue" << endl;
    getchar();

    SensorData dat(trajFile);
    TrajectoryDMPLearner learner(az, bz, dat.getRange(0, 8));
    Dmp leftDmp = learner.fitTrajectories();
    DMPExecutor leftExecutor(leftDmp, leftQueue);
    leftExecutor.executeTrajectory(ac, 0, leftDmp.getTmax(), dmpStepSize, tolAbsErr, tolRelErr);

    // first finger

    cout << "(main) ready to move first finger?" << endl;
    getchar();

    leftHandJoints = {0, -0.38705404571511043, 0.7258474179992682, 0.010410616072391092, -1.2259735578027993, -0.8303327436948519, 0.8185967300722126};
    leftHand->publishSdhJoints(leftHandJoints);

    leftHandJoints = {0, -0.38705404571511043, 0.7258474179992682, 0.010410616072391092, -1.2259735578027993, -0.8303327436948519, 0.5185967300722126};
    leftHand->publishSdhJoints(leftHandJoints);

    leftHandJoints = {0, -0.38705404571511043, 0.7258474179992682, 0.010410616072391092, -1.2259735578027993, -0.3303327436948519, 0.5185967300722126};
    leftHand->publishSdhJoints(leftHandJoints);

    leftHandJoints = {0, -0.38705404571511043, 0.7258474179992682, 0.010410616072391092, -1.2259735578027993, -0.3303327436948519, 1.285967300722126};
    leftHand->publishSdhJoints(leftHandJoints);

    cout << "(main) ready to move second finger?" << endl;
    getchar();

    // second finger follows
    leftHandJoints = {0, -0.8303327436948519, 0.8185967300722126, 0.010410616072391092, -1.2259735578027993, -0.3303327436948519, 1.285967300722126};
    leftHand->publishSdhJoints(leftHandJoints);

    leftHandJoints = {0, -0.8303327436948519, 0.5185967300722126, 0.010410616072391092, -1.2259735578027993, -0.3303327436948519, 1.285967300722126};
    leftHand->publishSdhJoints(leftHandJoints);

    leftHandJoints = {0, -0.3303327436948519, 0.5185967300722126, 0.010410616072391092, -1.2259735578027993, -0.3303327436948519, 1.285967300722126};
    leftHand->publishSdhJoints(leftHandJoints);

    leftHandJoints = {0, -0.3303327436948519, 1.285967300722126, 0.010410616072391092, -1.2259735578027993, -0.3303327436948519, 1.285967300722126};
    leftHand->publishSdhJoints(leftHandJoints);

    /*

    for(int i = 0; i < leftDmp.getDegreesOfFreedom(); ++i) {
        Gnuplot g1;
        g1.plot_xy(armadilloToStdVec(res.t), armadilloToStdVec(res.y.at(i)));
        g1.plot_xy(armadilloToStdVec(dat.getTime()), armadilloToStdVec(dat.getDataByIdx(i + 1)));
        getchar();
    }

    */

    /*

    vector<shared_ptr<ControlQueue>> queues = {leftQueue};
    vector<shared_ptr<GenericHand>> hands = {leftHand};
    SensorStorage store(queues, hands, 100);
    shared_ptr<thread> storageThread = store.startDataStorage(dataFolder);

    if(storageThread)
        storageThread->join();

    */

    leftQueue->setFinish();

    if(CONTROL_RIGHT)
        rightQueue->setFinish();

    lqThread->join();
    rqThread->join();

}