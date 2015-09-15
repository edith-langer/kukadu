######################
## Version 0.1 #######
######################

import sys, time
## import pickle
import numpy as np
## import scipy.io 
## import pylab as plt

## ##########################################
import mmr_base_classes
import mmr_setparams
import mmr_mmr_cls
import argparse
## from mmr_load_data_state import load_data_state
import trajlab_load_data
from mmr_validation import mmr_validation
from mmr_kernel import mmr_kernel
## from mmr_train import mmr_train
from mmr_test import inverse_knn
from mmr_report import mmr_report
from mmr_eval import mmr_eval_binvector
## from mmr_tools import mmr_cross_kernel
## from mmr_normalization_new import mmr_normalization
## ---------------------------------
## #################################
def mmr_main(iworkmode, trainingBase, evalFile):

  params=mmr_setparams.cls_params()
  params.setvalidation()
  params.setsolver()
  params.setgeneral()
  params.setoutput()
  params.setinput()
  params.setinputraw()
  
  np.set_printoptions(precision=4)
  
  dresult={}
  nview=1

  nobject=1

  ## !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  ## ################################################
  lviews=[0,1] ### this list could contain a subset of 0,1,2
  ## ################################################

  lresult=[]
  for iobject in range(nobject):

    for ifeature in range(nview):

      params.ninputview=len(lviews)
      cMMR=mmr_mmr_cls.cls_mmr(params.ninputview)
      nfold=cMMR.nfold
      nrepeat=cMMR.nrepeat
      ## cMMR.xbias=-0.06  ## 4 categories
      cMMR.xbias=0.02-ifeature*0.01 
      ## cMMR.xbias=0.1-ifeature*0.01 

      nscore=4
      nipar=1
      if cMMR.crossval_mode==0:   ## random
        nfold0=nfold
        xresult_test=np.zeros((nipar,nrepeat,nfold0))
        xresult_train=np.zeros((nipar,nrepeat,nfold0))
        xpr=np.zeros((nipar,nrepeat,nfold0,nscore))
      elif cMMR.crossval_mode==1:  ## predefined trianing and test
        nrepeat=1
        nfold0=1
        xresult_test=np.zeros((nipar,nrepeat,nfold0))
        xresult_train=np.zeros((nipar,nrepeat,nfold0))
        xpr=np.zeros((nipar,nrepeat,nfold0,nscore))

      cdata_store = trajlab_load_data.cls_label_files(trainingBase, evalFile)  
      cdata_store.load_mmr(cMMR, lviews)
      mdata=cMMR.mdata

      xcross=np.zeros((mdata,mdata))

      params.validation.rkernel=cMMR.XKernel[0].title

      xtime=np.zeros(5)
    ## ############################################################
      nparam=4    ## C,D,par1,par2
      xbest_param=np.zeros((nrepeat,nfold0,nparam))

      for iipar in range(nipar):
        
        for irepeat in range(nrepeat):
        ## split data into training and test
          if cMMR.crossval_mode==0:  ## random selection
            xselector=np.zeros(mdata)
            ifold=0
            for i in range(mdata):
              xselector[i]=ifold
              ifold+=1
              if ifold>=nfold0:
                ifold=0
            np.random.shuffle(xselector)
          elif cMMR.crossval_mode==1: ## preddefined training and test
			# (added by simon) train with all data but the last one (not elegant, but works)
            cMMR.ifixtrain = list(range(mdata - 1))
            xselector = np.zeros(mdata)
            xselector[cMMR.ifixtrain] = 1

          for ifold in range(nfold0):
            cMMR.split_train_test(xselector,ifold)

            ## validation to choose the best parameters
            t0 = time.clock()
            ## select the kernel to be validated
            cMMR.set_validation()
            ## params.validation.rkernel=cMMR.XKernel[0].title
            if params.validation.rkernel in cMMR.dkernels:
              kernbest = cMMR.dkernels[params.validation.rkernel].kernel_params
            else:
              kernbest = cMMR.XKernel[0].kernel_params

            if params.validation.ivalid == 1:
              best_param = mmr_validation(cMMR,params)
            else:
              best_param = mmr_base_classes.cls_empty_class()
              best_param.par1 = kernbest.ipar1
              best_param.par2 = kernbest.ipar2
              ## common params
              best_param.c = cMMR.penalty.c
              best_param.d = cMMR.penalty.d
              
            xtime[0] = time.clock() - t0
            xbest_param[irepeat,ifold,0]=best_param.c
            xbest_param[irepeat,ifold,1]=best_param.d
            xbest_param[irepeat,ifold,2]=best_param.par1
            xbest_param[irepeat,ifold,3]=best_param.par2

            kernbest.ipar1=best_param.par1
            kernbest.ipar2=best_param.par2
			## common params
            cMMR.penalty.c=best_param.c
            cMMR.penalty.d=best_param.d
            cMMR.compute_kernels()
            cMMR.Y0=cMMR.YKernel.get_train(cMMR.itrain)   ## candidates
              
            
            t0=time.clock()
            cOptDual=cMMR.mmr_train(params)
            xtime[1]=time.clock()-t0
      ## cls transfers the dual variables to the test procedure
      ## compute tests 
      ## check the train accuracy
            cPredictTra = cMMR.mmr_test(cOptDual,params,itraindata=0)
      ## counts the proportion the ones predicted correctly    
      ## ######################################
            if cMMR.itestmode==2:
              ypred=inverse_knn(cMMR.YKernel.get_Y0(cMMR.itrain), \
                                cPredictTra)
            else:
              ypred=cPredictTra.zPred
            cEvaluationTra= \
                  mmr_eval_binvector(cMMR.YKernel.get_train(cMMR.itrain), \
                                     ypred)
            xresult_train[iipar,irepeat,ifold]=cEvaluationTra.accuracy
      ## ######################################     
      ## check the test accuracy
            t0=time.clock()
            cPredictTes = cMMR.mmr_test(cOptDual,params,itraindata=1)
      ## counts the proportion the ones predicted correctly
            if cMMR.itestmode==2:
              ypred=inverse_knn(cMMR.YKernel.get_Y0(cMMR.itrain), \
                                cPredictTes)
            else:
              ypred=cPredictTes.zPred
            ## cEvaluationTes=mmr_eval_binvector(cData.YTest,cPredictTes.zPred)
            cEvaluationTes= \
                  mmr_eval_binvector(cMMR.YKernel.get_test(cMMR.itest), \
                                     ypred)

            xtime[2] = time.clock() - t0
            xresult_test[iipar,irepeat,ifold] = cEvaluationTes.accuracy

            xpr[iipar,irepeat,ifold,0]=cEvaluationTes.precision
            xpr[iipar,irepeat,ifold,1]=cEvaluationTes.recall
            xpr[iipar,irepeat,ifold,2]=cEvaluationTes.f1
            xpr[iipar,irepeat,ifold,3]=cEvaluationTes.accuracy

            # (added by simon) for now i will just add the new data to
            # the dataset with a random label and check the confusion
            # matrix --> very ugly solution but i cant figure it out
            # in a clean way
            # print(cEvaluationTes.classconfusion)
            evaluatedRes = [row[0] for row in cEvaluationTes.classconfusion]
            nonZeroIndexes = [i for i, e in enumerate(evaluatedRes) if e != 0]
            print(evaluatedRes)
            return evaluatedRes
            #return nonZeroIndexes[0]
            try:
              xclassconfusion+=cEvaluationTes.classconfusion
            except:
              (n,n)=cEvaluationTes.classconfusion.shape
              xclassconfusion=np.zeros((n,n))
              xclassconfusion+=cEvaluationTes.classconfusion
            ## mmr_eval_label(ZW,iPre,YTesN,Y0,kit_data,itest,params)




        sys.stdout.flush()

        dresult[ifeature]=(cMMR.xbias,np.mean(np.mean(xpr[iipar,:,:,:],0),0))

    for sfeature_type,tresult in dresult.items():
      ## xhead=cMMR.xbias
      xhead=''
      lresult.append((xhead,tresult))
  
  return -1

def runClassifier(trainingBase, evalFile, pcl, bestParamC, bestParamD, bestParamParam1, bestParamParam2):
  iworkmode = 0
  print(trainingBase)
  print(evalFile)
  return mmr_main(iworkmode, trainingBase, evalFile)

