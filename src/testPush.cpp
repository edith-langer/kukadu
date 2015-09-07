
#include "../include/kukadu.h"

#include <memory>
#include <iostream>
#include <boost/program_options.hpp>

namespace po = boost::program_options;
using namespace std;
using namespace arma;

int importanceSamplingCount;
double tau, az, bz, dmpStepSize, tolAbsErr, tolRelErr, ac, as, alpham;
string inDir, cfFile, dataFolder, trajFile;
vector<double> rlExploreSigmas;

string environment = "real";
std::string hand = "left";


int main(int argc, char** args) {



    //    string saveDataPath = "/home/c7031098/testing/SimData/TwoFingers_raw/pushTransLeftToRight_planePart/";
    //    string loadDataPath = "/home/c7031098/testing/RealData/TwoFingers_raw/pushTransLeftToRight_planePart/kuka_lwr_real_left_arm_0";
    //    string loadObjectDataPath = "/home/c7031098/testing/RealData/TwoFingers_raw/pushTransLeftToRight_planePart/vision";
    string loadDataPath = "/home/c7031098/testing/data_gen/RealData/raw/kuka_lwr_real_left_arm_0";

    string saveDataPath = "/home/c7031098/testing/data_gen/RealData/boxNew";


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


    shared_ptr<ControlQueue> leftQueue = shared_ptr<ControlQueue>(new KukieControlQueue(kukaStepWaitTime, environment, "left_arm", *node));
    vector<shared_ptr<ControlQueue>> queueVectors;
    queueVectors.push_back(leftQueue);

        RosSchunk* handQ=new RosSchunk(*node, environment, hand);
        cout<<"hand interface created"<<endl;

        //moving hand to staring position
        vector<double> newPos = {0, -1.57, 0, -1.57, 0, -1.57, 0};
        handQ->publishSdhJoints(newPos);


    //moving arm
    leftQueue->stopCurrentMode();

    shared_ptr<thread> lqThread = leftQueue->startQueueThread();
    leftQueue->switchMode(10);


    shared_ptr<SensorData> data = SensorStorage::readStorage(leftQueue, loadDataPath);
    data->removeDuplicateTimes();

    //    shared_ptr<SensorData> dataO = SensorStorage::readStorage(leftQueue, loadObjectDataPath);
    //    dataO->removeDuplicateTimes();


    //loading data for learner
    arma::vec times = data->getTimes();
    arma::mat jointPos = data->getJointPos();
    arma::mat cartPos = data->getCartPos();
    cout <<"  data loaded" <<endl;

    CartesianDMPLearner learner(az, bz, join_rows(times, cartPos));
    std::shared_ptr<Dmp> leftDmp = learner.fitTrajectories();
    shared_ptr<thread> storeThread;

    leftQueue->moveJoints(jointPos.row(0).t());

    leftQueue->stopCurrentMode();
    leftQueue->setStiffness(1800, 150, 0.7, 7.0,70.0, 2.0);
    leftQueue->switchMode(20);



    if(environment == "simulation"){

        //creatin simulation interface
        std::shared_ptr<SimInterface> simI= std::shared_ptr<SimInterface>(new SimInterface (argc, args, kukaStepWaitTime, *node));
        cout<<"simulator interface created"<<endl;

        //   float newP[3] = {0.5, 0.7, 0.01}; //orig

        simI->addPrimShape(1,"sponge", {0.6, 0.7, 0.01}, {0, 0, 0, 0}, {1, 2 , 0.08}, 10.0);
        simI->setObjMaterial("sponge", SimInterface::FRICTION_HIGH);
        cout<< "sponge imported "<< endl;

        string objectId = "box";
        simI->addPrimShape(1, objectId, {0.30, 0.7, 0.2}, {0, 0, 0, 0}, {0.2, 0.2, 0.1}, 1);
        cout <<"box imported " << endl;


        DMPExecutorPush leftExecutor(leftDmp, leftQueue, environment, simI, objectId);

        //leftExecutor.setObjectData(dataO->getTimes(), dataO->getCartPos());
        leftExecutor.setObjectData(data->getTimes(), data->getCartPos());

        SensorStorage storeData(queueVectors, std::vector<std::shared_ptr<GenericHand>>(), simI, objectId, 1000);
        storeData.setExportMode(STORE_TIME | STORE_RBT_CART_POS | STORE_RBT_JNT_POS | STORE_SIM_OBJECT);

        storeThread = storeData.startDataStorage(saveDataPath);

        leftExecutor.executeTrajectory(ac, 0, leftDmp->getTmax(), dmpStepSize, tolAbsErr, tolRelErr);
        storeData.stopDataStorage();

        storeThread->join();
        leftExecutor.saveData(saveDataPath);



    }
    if (environment == "real"){

        shared_ptr<VisionInterface> vis = shared_ptr<VisionInterface>(new VisionInterface(kukaStepWaitTime, *node));
        SensorStorage storeData(std::vector<std::shared_ptr<ControlQueue>>(), std::vector<std::shared_ptr<GenericHand>>(), vis, 1000);
        storeData.setExportMode(STORE_TIME | STORE_RBT_CART_POS | STORE_RBT_JNT_POS | STORE_VIS_OBJECT);

        vis->setArTagTracker();
        cout<<"ArTagTracke started"<<endl;

        DMPExecutorPush leftExecutor(leftDmp, leftQueue, environment, vis);

        //        while(1){
        //        cout<<leftQueue->getCartesianPose()<<endl;
        //        sleep(0.5);
        //        }

        sleep(1.0);

        leftExecutor.setObjectData(data->getTimes(), data->getCartPos());


        storeThread = storeData.startDataStorage(saveDataPath);

        cout <<" (collectData) executing starting" <<endl;

        leftExecutor.executeTrajectory(ac, 0, leftDmp->getTmax(), dmpStepSize, tolAbsErr, tolRelErr);

        leftQueue->stopCurrentMode();
        leftExecutor.stopObject();

        cout<<" (collectData) execution done "<<endl;
        storeData.stopDataStorage();

        storeThread->join();
        leftExecutor.saveData(saveDataPath);
    }



    cout<<"(collectData) moving to start position "<<endl;

    leftQueue->stopCurrentMode();
    leftQueue->switchMode(10);
    leftQueue->moveJoints(jointPos.row(0).t());
    // leftQueue->moveCartesian(vectorarma2pose(&startP));
    leftQueue->stopCurrentMode();

    getchar();


    return 0;


}
