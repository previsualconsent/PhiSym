import FWCore.ParameterSet.Config as cms

PhiSymCalibration = cms.EDAnalyzer(
    "PhiSymCalibration",
    infoTag = cms.untracked.InputTag("PhiSymProducer", "", "PHISYM"),
    recHitEBTag = cms.untracked.InputTag("PhiSymProducer", "EB", "PHISYM"),
    recHitEETag = cms.untracked.InputTag("PhiSymProducer", "EE", "PHISYM"),
    oldCalibFile = cms.untracked.string(""),
    absCalibFile = cms.untracked.string(""),
    blocksToSum = cms.untracked.int32(150),
    computeICs = cms.untracked.bool(True)
)
