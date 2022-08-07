#include "ROOT/RDataFrame.hxx"
#include "ROOT/RVec.hxx"
#include "TCanvas.h"
#include "TH1D.h"
#include "TLatex.h"
#include "TStyle.h"
 
#include <iostream>
#include "TStopwatch.h"

#include "ROOT/TTreeProcessorMT.hxx"

//#include <ROOT/RLogger.hxx>

//auto verbosity = ROOT::Experimental::RLogScopedVerbosity(ROOT::Detail::RDF::RDFLogChannel(), ROOT::Experimental::ELogLevel::kInfo);


using namespace ROOT::VecOps;
 
void df102_NanoAODDimuonAnalysis(int nfiles, bool numaopt)
{
   // Enable multi-threading
   ROOT::EnableImplicitMT();
   ROOT::TTreeProcessorMT::SetNUMAOPT(numaopt);
 
   // Create dataframe from NanoAOD files
   ROOT::RDataFrame df("Events", std::vector<std::string>(nfiles, "INPUT/Run2012BC_DoubleMuParked_Muons.root"));
 
   // For simplicity, select only events with exactly two muons and require opposite charge
   auto df_2mu = df.Filter([](unsigned int n) { return n == 2; }, {"nMuon"}, "Events with exactly two muons");
   auto df_os = df_2mu.Filter([](const ROOT::RVec<int> &ch) { return ch[0] != ch[1]; }, {"Muon_charge"}, "Muons with opposite charge");
 
   // Compute invariant mass of the dimuon system
   auto df_mass = df_os.Define("Dimuon_mass", InvariantMass<float>, {"Muon_pt", "Muon_eta", "Muon_phi", "Muon_mass"});
 
   // Make histogram of dimuon mass spectrum. Note how we can set title and axis labels in one go
   auto h = df_mass.Histo1D<float>({"Dimuon_mass", "Dimuon mass;m_{#mu#mu} (GeV);N_{Events}", 30000, 0.25, 300}, "Dimuon_mass");
 
   // Request cut-flow report
   auto report = df_mass.Report();
 
   // Produce plot
   gStyle->SetOptStat(0); gStyle->SetTextFont(42);
   auto c = new TCanvas("c", "", 800, 700);
   c->SetLogx(); c->SetLogy();
 
   h->GetXaxis()->SetTitleSize(0.04);
   h->GetYaxis()->SetTitleSize(0.04);
   h->DrawClone();
 
   TLatex label; label.SetNDC(true);
   label.DrawLatex(0.175, 0.740, "#eta");
   label.DrawLatex(0.205, 0.775, "#rho,#omega");
   label.DrawLatex(0.270, 0.740, "#phi");
   label.DrawLatex(0.400, 0.800, "J/#psi");
   label.DrawLatex(0.415, 0.670, "#psi'");
   label.DrawLatex(0.485, 0.700, "Y(1,2,3S)");
   label.DrawLatex(0.755, 0.680, "Z");
   label.SetTextSize(0.040); label.DrawLatex(0.100, 0.920, "#bf{CMS Open Data}");
   label.SetTextSize(0.030); label.DrawLatex(0.630, 0.920, "#sqrt{s} = 8 TeV, L_{int} = 11.6 fb^{-1}");
 
   c->SaveAs("dimuon_spectrum.pdf");
 
   // Print cut-flow report
   report->Print();
}
 
int main(int argc, const char* argv[])
{
  int nf = atoi(argv[1]);
  bool numaopt = (argc>2) ? bool(atoi(argv[2])): false;

  TStopwatch s;
  s.Start();
  df102_NanoAODDimuonAnalysis(nf, numaopt);
  s.Stop();
  std::cout << "Elapsed (df102): " << s.RealTime() << std::endl;
}
