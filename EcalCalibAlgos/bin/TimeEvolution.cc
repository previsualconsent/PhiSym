#ifndef __TIME_EVOLUTION__
#define __TIME_EVOLUTION__

#include <map>
#include <vector>
#include <string>
#include <fstream>

#include "TSystem.h"
#include "TMath.h"
#include "TFile.h"
#include "TTree.h"
#include "TF1.h"
#include "TH1F.h"
#include "TH2F.h"
#include "TGraphErrors.h"

#include "FWCore/FWLite/interface/AutoLibraryLoader.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/PythonParameterSet/interface/MakeParameterSets.h"

#include "DataFormats/EcalDetId/interface/EBDetId.h"
#include "DataFormats/EcalDetId/interface/EEDetId.h"

#include "Calibration/Tools/interface/EcalRingCalibrationTools.h"

#include "PhiSym/EcalCalibDataFormats/interface/PhiSymRecHit.h"
#include "PhiSym/EcalCalibDataFormats/interface/CalibrationFile.h"
#include "PhiSym/EcalCalibAlgos/interface/utils.h"

using namespace std;

//**********MAIN**************************************************************************
int main(int argc, char *argv[])
{
    AutoLibraryLoader::enable();        
    gSystem->Load("libFWCoreFWLite.so"); 
    gSystem->Load("libDataFormatsFWLite.so");
    gSystem->Load("libDataFormatsEcalDetId.so");
    gSystem->Load("libPhiSymEcalCalibDataFormats.so");

    //---Setup---
    if(argc < 2)
    {
        cout << "Usage : " << argv[0] << " [parameters.py]" << endl;
        return 0;
    }
    if(!edm::readPSetsFrom(argv[1])->existsAs<edm::ParameterSet>("process"))
    {
        cout << " ERROR: ParametersSet 'process' is missing in your configuration file"
             << endl;
        return 0;
    }
    
    //---get the python configuration
    const edm::ParameterSet &process = edm::readPSetsFrom(argv[1])->getParameter<edm::ParameterSet>("process");    
    bool applyCorr = process.getParameter<bool >("applyCorrections");
    vector<string> types = process.getParameter<vector<string> >("variables");

    const edm::ParameterSet &filesOpt = process.getParameter<edm::ParameterSet>("ioFilesOpt");    
    vector<string> files = filesOpt.getParameter<vector<string> >("inputFiles");       
    vector<string> corrections_files = filesOpt.getParameter<vector<string> >("correctionsFiles");       

    map<string, map<int, vector<float> > > ebVar;
    map<string, map<int, vector<float> > > eeVar;
    pair<int, int> ebMap[EBDetId::kSizeForDenseIndexing];
    //int eeMapRing[EEDetId::kSizeForDenseIndexing];
    pair<int, int> eeMapXY[EEDetId::kSizeForDenseIndexing];

    for(unsigned int iFile=0; iFile<files.size(); ++iFile)
    {
        TFile* file = TFile::Open(files[iFile].c_str(), "READ");
        if(!file)
            continue;
        CrystalsEBTree ebTree((TTree*)file->Get("eb_xstals"));
        CrystalsEETree eeTree((TTree*)file->Get("ee_xstals"));

        //---allocate enough memory
        vector<float> ebCorr;
        ebCorr.resize(EBDetId::kSizeForDenseIndexing);
        for(auto& type : types)
        {
            ebVar[type][iFile].resize(EBDetId::kSizeForDenseIndexing);
            eeVar[type][iFile].resize(EEDetId::kSizeForDenseIndexing);
        }
        
        int index=-1;
        long int tot_hits_EB=0;
        long int tot_hits_EE=0;

        //---get geo&material correction if type == ic
        if(applyCorr)
        {
            if(corrections_files.size() < files.size())
            {
                cout << "ERROR: too few corrections files provided" << endl;
                return 0;
            }
            
            int ieta, iphi, side;
            float corr;            
            ifstream corrections(corrections_files[iFile], ios::in);
            while(corrections.good())
            {
                corrections >> ieta >> iphi >> side >> corr;
                ebCorr[EBDetId(ieta, iphi).hashedIndex()] = corr;
            }
            corrections.close();
        }

        //---EB
        while(ebTree.NextEntry())
        {
            if(ebTree.rec_hit->GetNhits() > 0)
                tot_hits_EB += ebTree.rec_hit->GetNhits();
            index=EBDetId(ebTree.ieta, ebTree.iphi).hashedIndex();
            for(auto& type : types)
            {
                if(type == "IC")
                {
                    ebVar[type][iFile][index] = ebTree.ic_abs*ebTree.ic_ch;
                    if(applyCorr)
                        ebVar[type][iFile][index] *= ebCorr[index];
                }
                if(type == "LC")
                    ebVar[type][iFile][index] = ebTree.rec_hit->GetLCSum()/ebTree.rec_hit->GetNhits();
                if(type == "SumEt")
                    ebVar[type][iFile][index] = ebTree.rec_hit->GetSumEt(0)/ebTree.rec_hit->GetNhits();
                if(type == "Nhits")
                    ebVar[type][iFile][index] = ebTree.rec_hit->GetNhits();
                if(type == "Kfact")
                    ebVar[type][iFile][index] = ebTree.k_ch;
                if(type == "Corr")
                    ebVar[type][iFile][index] = ebCorr[index];
            }
            if(iFile==0)
                ebMap[index] = make_pair(ebTree.ieta, ebTree.iphi);
        }
        //---EE
        while(eeTree.NextEntry())
        {
            if(eeTree.rec_hit->GetNhits() > 0)
                tot_hits_EE += eeTree.rec_hit->GetNhits();
            index=EEDetId(eeTree.ix, eeTree.iy, eeTree.iring>0?1:-1).hashedIndex();
            for(auto& type : types)
            {
                if(type == "IC")
                    eeVar[type][iFile][index] = eeTree.ic_abs*eeTree.ic_ch;
                if(type == "LC")
                    eeVar[type][iFile][index] = eeTree.rec_hit->GetLCSum()/eeTree.rec_hit->GetNhits();
                if(type == "SumEt")
                    eeVar[type][iFile][index] = eeTree.rec_hit->GetSumEt(0)/eeTree.rec_hit->GetNhits();
                if(type == "Nhits")
                    eeVar[type][iFile][index] = eeTree.rec_hit->GetNhits();
                if(type == "Kfact")
                    eeVar[type][iFile][index] = eeTree.k_ch;
                if(type == "Corr")
                    eeVar[type][iFile][index] = 1;
            }
            if(iFile==0)
            {
                //eeMapRing[index] = eeTree.iring<0? eeTree.iring+39 : eeTree.iring+38;
                eeMapXY[index] = make_pair(eeTree.iring<0 ? -eeTree.ix : eeTree.ix, eeTree.iy);
            }
        }
        
        for(auto& type : types)
        {
            if(type == "Nhits")
                continue;

            float sum=0;
            for(int index=0; index<EBDetId::kSizeForDenseIndexing; ++index)
                sum += ebVar[type][iFile][index];
            for(int index=0; index<EBDetId::kSizeForDenseIndexing; ++index)
                ebVar[type][iFile][index] = ebVar[type][iFile][index]/sum;
            sum=0;
            for(int index=0; index<EEDetId::kSizeForDenseIndexing; ++index)
                sum += eeVar[type][iFile][index];
            for(int index=0; index<EEDetId::kSizeForDenseIndexing; ++index)
                eeVar[type][iFile][index] = eeVar[type][iFile][index]/sum;
        }
        
        cout << files[iFile] << " nhits/crystal (EB/EE): " << tot_hits_EB/71200. << "  " << tot_hits_EE/14000. << endl;
        file->Close();
    }

    //---compute & draw
    for(auto& type : types)
    {
        TFile* outFile = new TFile(string(type+"_ratios_vs_time.root").c_str(), "RECREATE");
        outFile->cd();
        //---EB
        TH1F* hAbsEB = new TH1F("hAbsEB", "Absolute #sigma_{ratio} spread --- barrel", 250, 0, 0.1);
        TH1F* hRelEB = new TH1F("hRelEB", "Relative #sigma_{ratio} spread --- barrel", 250, 0, 0.1);
        TGraphErrors* grAbsEB = new TGraphErrors();
        TGraphErrors* grRelEB = new TGraphErrors();
        TF1* fitFuncAbsEB = new TF1("fitFuncAbsEB", "gaus", 0.9, 1.1);
        TF1* fitFuncRelEB = new TF1("fitFuncRelEB", "gaus", 0.98, 1.02);
        //---EE
        TH1F* hAbsEE = new TH1F("hAbsEE", "Absolute #sigma_{ratio} spread --- endcaps", 250, 0, 0.5);
        TH1F* hRelEE = new TH1F("hRelEE", "Relative #sigma_{ratio} spread --- endcaps", 250, 0, 0.5);
        TGraphErrors* grAbsEE = new TGraphErrors();
        TGraphErrors* grRelEE = new TGraphErrors();
        TF1* fitFuncAbsEE = new TF1("fitFuncAbsEE", "gaus", 0.8, 1.2);
        TF1* fitFuncRelEE = new TF1("fitFuncRelEE", "gaus", 0.8, 1.2);

        float mapAbsEB_range[2];
        float mapRelEB_range[2];
        float mapAbsEE_range[2];
        float mapRelEE_range[2];
    
        //---fill histos
        for(unsigned int iFile=1; iFile<files.size(); ++iFile)
        {
            TH1F* tmpAbsEB = new TH1F("tmpAbsEB", "", 2000, 0.5, 1.5);
            TH1F* tmpRelEB = new TH1F("tmpRelEB", "", 2000, 0.5, 1.5);
            TH1F* tmpAbsEE = new TH1F("tmpAbsEE", "", 1000, 0.5, 1.5);
            TH1F* tmpRelEE = new TH1F("tmpRelEE", "", 1000, 0.5, 1.5);
            TH2F* mapAbsEB = new TH2F("mapAbsEB", "", 360, 0.5, 360.5, 171, -85, 85);        
            TH2F* mapRelEB = new TH2F("mapRelEB", "", 360, 0.5, 360.5, 171, -85, 85);
            TH2F* mapAbsEE = new TH2F("mapAbsEE", "", 201, -100.5, 100.5, 101, 0.5, 100.5);
            TH2F* mapRelEE = new TH2F("mapRelEE", "", 201, -100.5, 100.5, 101, 0.5, 100.5);
            mapAbsEB->SetContour(100);
            mapRelEB->SetContour(100);
            mapAbsEE->SetContour(100);
            mapRelEE->SetContour(100);
            //---EB
            for(int index=0; index<EBDetId::kSizeForDenseIndexing; ++index)
            {
                if(ebVar[type][0][index] != 0 && ebVar[type][iFile][index] != 0)
                {
                    tmpAbsEB->Fill(ebVar[type][iFile][index]/ebVar[type][0][index]);
                    mapAbsEB->Fill(ebMap[index].second, ebMap[index].first, ebVar[type][iFile][index]/ebVar[type][0][index]);
                }
                if(ebVar[type][iFile-1][index] != 0 && ebVar[type][iFile][index] != 0)
                {
                    tmpRelEB->Fill(ebVar[type][iFile][index]/ebVar[type][iFile-1][index]);
                    mapRelEB->Fill(ebMap[index].second, ebMap[index].first, ebVar[type][iFile][index]/ebVar[type][iFile-1][index]);
                }
            }
            //---EE
            for(int index=0; index<EEDetId::kSizeForDenseIndexing; ++index)
            {
                if(eeVar[type][0][index] != 0 && eeVar[type][iFile][index] != 0)
                {
                    tmpAbsEE->Fill(eeVar[type][iFile][index]/eeVar[type][0][index]);
                    mapAbsEE->Fill(eeMapXY[index].first, eeMapXY[index].second, eeVar[type][iFile][index]/eeVar[type][0][index]);
                }
                if(eeVar[type][iFile-1][index] != 0 && eeVar[type][iFile][index] != 0)
                {
                    tmpRelEE->Fill(eeVar[type][iFile][index]/eeVar[type][iFile-1][index]);
                    mapRelEE->Fill(eeMapXY[index].first, eeMapXY[index].second, eeVar[type][iFile][index]/eeVar[type][iFile-1][index]);
                }       
            }
            mapAbsEB_range[0] = tmpAbsEB->GetMean()-2*tmpAbsEB->GetRMS();
            mapAbsEB_range[1] = tmpAbsEB->GetMean()+2*tmpAbsEB->GetRMS();
            mapRelEB_range[0] = tmpRelEB->GetMean()-2*tmpRelEB->GetRMS();
            mapRelEB_range[1] = tmpRelEB->GetMean()+2*tmpRelEB->GetRMS();
            mapAbsEE_range[0] = tmpAbsEE->GetMean()-2*tmpAbsEE->GetRMS();
            mapAbsEE_range[1] = tmpAbsEE->GetMean()+2*tmpAbsEE->GetRMS();
            mapRelEE_range[0] = tmpRelEE->GetMean()-2*tmpRelEE->GetRMS();
            mapRelEE_range[1] = tmpRelEE->GetMean()+2*tmpRelEE->GetRMS();
        
            //---EB                     
            hAbsEB->Fill(fitFuncAbsEB->GetParameter(2));
            grAbsEB->SetPoint(iFile-1, iFile, PhiSym::EffectiveSigma(tmpAbsEB));
            grAbsEB->SetPointError(iFile-1, 0, tmpAbsEB->GetRMSError());
            hRelEB->Fill(fitFuncRelEB->GetParameter(2));
            grRelEB->SetPoint(iFile-1, iFile, PhiSym::EffectiveSigma(tmpRelEB));
            grRelEB->SetPointError(iFile-1, 0, tmpRelEB->GetRMSError());
            tmpAbsEB->Write(string("AbsEB_"+to_string(iFile)).c_str());
            tmpRelEB->Write(string("RelEB_"+to_string(iFile)).c_str());        
            mapAbsEB->SetAxisRange(mapAbsEB_range[0], mapAbsEB_range[1], "Z");
            mapRelEB->SetAxisRange(mapRelEB_range[0], mapRelEB_range[1], "Z");
            mapAbsEB->Write(string("mapAbsEB_"+to_string(iFile)).c_str());
            mapRelEB->Write(string("mapRelEB_"+to_string(iFile)).c_str());
            tmpAbsEB->Delete();
            tmpRelEB->Delete();
            mapAbsEB->Delete();
            mapRelEB->Delete();
            //---EE
            hAbsEE->Fill(fitFuncAbsEE->GetParameter(2));
            grAbsEE->SetPoint(iFile-1, iFile, PhiSym::EffectiveSigma(tmpAbsEE));
            grAbsEE->SetPointError(iFile-1, 0, tmpAbsEE->GetRMSError());
            hRelEE->Fill(fitFuncRelEE->GetParameter(2));
            grRelEE->SetPoint(iFile-1, iFile, PhiSym::EffectiveSigma(tmpRelEE));
            grRelEE->SetPointError(iFile-1, 0, tmpRelEE->GetRMSError());
            tmpAbsEE->Write(string("AbsEE_"+to_string(iFile)).c_str());
            tmpRelEE->Write(string("RelEE_"+to_string(iFile)).c_str()); 
            mapAbsEE->SetAxisRange(mapAbsEE_range[0], mapAbsEE_range[1], "Z");
            mapRelEE->SetAxisRange(mapRelEE_range[0], mapRelEE_range[1], "Z");
            mapAbsEE->Write(string("mapAbsEE_"+to_string(iFile)).c_str());
            mapRelEE->Write(string("mapRelEE_"+to_string(iFile)).c_str());
            tmpAbsEE->Delete();
            tmpRelEE->Delete();
            mapAbsEE->Delete();
            mapRelEE->Delete();
        }
        //---EB
        hAbsEB->SetFillColor(kBlue-4);
        hRelEB->SetFillColor(kRed-4);
        hAbsEB->Write("hAbsEB");
        hRelEB->Write("hRelEB");
        grAbsEB->SetMarkerColor(kBlue);
        grAbsEB->SetMarkerStyle(20);
        grAbsEB->SetMarkerSize(0.7);
        grRelEB->SetMarkerColor(kRed);
        grRelEB->SetMarkerStyle(20);
        grRelEB->SetMarkerSize(0.7);
        grAbsEB->Write("grAbsEB");
        grRelEB->Write("grRelEB");
        //---EE
        hAbsEE->SetFillColor(kBlue-4);
        hRelEE->SetFillColor(kRed-4);
        hAbsEE->Write("hAbsEE");
        hRelEE->Write("hRelEE");
        grAbsEE->SetMarkerColor(kBlue);
        grAbsEE->SetMarkerStyle(20);
        grAbsEE->SetMarkerSize(0.7);
        grRelEE->SetMarkerColor(kRed);
        grRelEE->SetMarkerStyle(20);
        grRelEE->SetMarkerSize(0.7);
        grAbsEE->Write("grAbsEE");
        grRelEE->Write("grRelEE");
    
        outFile->Close();
    }
}

#endif

