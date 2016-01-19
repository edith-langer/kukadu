#ifndef KUKADU_GRADIENTDESCENT_H
#define KUKADU_GRADIENTDESCENT_H

#include <vector>
#include <cstdlib>
#include <utility>
#include <iostream>
#include <armadillo>

#include "CostComputer.h"
#include "GeneralReinforcer.h"
#include "../../utils/types.h"
#include "../../types/Trajectory.h"
#include "../../types/KukaduTypes.h"
#include "../../robot/ControlQueue.h"
#include "../../types/DictionaryTrajectory.h"
#include "../../trajectory/TrajectoryExecutor.h"

namespace kukadu {

    class GradientDescent : public GeneralReinforcer {

    private:

        int updateNum;
        int updatesPerRollout;
        int importanceSamplingCount;

        double explorationSigma;

        TrajectoryExecutor* trajEx;

        kukadu_mersenne_twister generator;

        std::vector<double> sigmas;
        std::vector<kukadu_normal_distribution> normals;
        std::vector<KUKADU_SHARED_PTR<Trajectory> > initDmp;
        std::vector<std::pair <double, KUKADU_SHARED_PTR<Trajectory> > > sampleHistory;

        void construct(std::vector<KUKADU_SHARED_PTR<Trajectory> > initDmp, std::vector<double> explorationSigmas, int updatesPerRollout, int importanceSamplingCount, KUKADU_SHARED_PTR<CostComputer> cost, KUKADU_SHARED_PTR<ControlQueue> simulationQueue, KUKADU_SHARED_PTR<ControlQueue> executionQueue, double ac, double dmpStepSize, double tolAbsErr, double tolRelErr);

    public:

        GradientDescent(KUKADU_SHARED_PTR<TrajectoryExecutor> trajEx, std::vector<KUKADU_SHARED_PTR<Trajectory> > initDmp, double explorationSigma, int updatesPerRollout, int importanceSamplingCount, KUKADU_SHARED_PTR<CostComputer> cost, KUKADU_SHARED_PTR<ControlQueue> simulationQueue, KUKADU_SHARED_PTR<ControlQueue> executionQueue, double ac, double dmpStepSize, double tolAbsErr, double tolRelErr);
        GradientDescent(KUKADU_SHARED_PTR<TrajectoryExecutor> trajEx, std::vector<KUKADU_SHARED_PTR<Trajectory> > initDmp, std::vector<double> explorationSigmas, int updatesPerRollout, int importanceSamplingCount, KUKADU_SHARED_PTR<CostComputer> cost, KUKADU_SHARED_PTR<ControlQueue> simulationQueue, KUKADU_SHARED_PTR<ControlQueue> executionQueue, double ac, double dmpStepSize, double tolAbsErr, double tolRelErr);

        KUKADU_SHARED_PTR<Trajectory> updateStep();

        std::vector<KUKADU_SHARED_PTR<Trajectory> > getInitialRollout();
        std::vector<KUKADU_SHARED_PTR<Trajectory> > computeRolloutParamters();

    };

}

#endif
