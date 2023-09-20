/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/* *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Michele Polese <michele.polese@gmail.com>
 */

#define RSEED 3091
#define TWO_PLMN

#include "ns3/quic-module.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"
#include "ns3/random-variable-stream.h"
#include "ns3/flow-monitor-module.h"

#include "ns3/buildings-helper.h"
#include "ns3/buildings-module.h"
#include "ns3/command-line.h"
#include "ns3/epc-helper.h"
#include "ns3/global-value.h"
#include "ns3/isotropic-antenna-model.h"
#include "ns3/mmwave-helper.h"
#include "ns3/mmwave-point-to-point-epc-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/node-list.h"
#include "ns3/lte-ue-net-device.h"
#include "ns3/rng-seed-manager.h"



#include <ctime>
#include <iostream>
#include <list>
#include <stdlib.h>

using namespace ns3;
using namespace mmwave;

/**
 * Sample simulation script for MC devices.
 * One LTE and two MmWave eNodeBs are instantiated, and a MC UE device is placed between them.
 * In particular, the UE is initially closer to one of the two BSs, and progressively moves towards
 * the other. During the course of the simulation multiple handovers occur, due to the changing
 * distance between the devices and the presence of obstacles, which can obstruct the LOS path
 * between the UE and the eNBs.
 */

NS_LOG_COMPONENT_DEFINE("MpquicTwoPlmn");

void ThroughputMonitor (FlowMonitorHelper *fmhelper, Ptr<FlowMonitor> flowMon, Ptr<OutputStreamWrapper> stream)
{
    std::map<FlowId, FlowMonitor::FlowStats> flowStats = flowMon->GetFlowStats();
    Ptr<Ipv4FlowClassifier> classing = DynamicCast<Ipv4FlowClassifier> (fmhelper->GetClassifier());
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator stats = flowStats.begin (); stats != flowStats.end (); ++stats)
    {
        if (stats->first == 1 || stats->first == 2){
            *stream->GetStream () << stats->first  << "\t" << Simulator::Now().GetSeconds()/*->second.timeLastRxPacket.GetSeconds()*/ << "\t" << stats->second.rxBytes << "\t" << stats->second.rxPackets << "\t" << stats->second.lastDelay.GetMilliSeconds() << "\t" << stats->second.rxBytes*8/1024/1024/(stats->second.timeLastRxPacket.GetSeconds()-stats->second.timeFirstRxPacket.GetSeconds())  << std::endl;
        }
    }
    Simulator::Schedule(Seconds(0.05),&ThroughputMonitor, fmhelper, flowMon, stream);
}

void
PrintGnuplottableBuildingListToFile(std::string filename)
{
    std::ofstream outFile;
    outFile.open(filename.c_str(), std::ios_base::out | std::ios_base::trunc);
    if (!outFile.is_open())
    {
        NS_LOG_ERROR("Can't open file " << filename);
        return;
    }
    uint32_t index = 0;
    for (BuildingList::Iterator it = BuildingList::Begin(); it != BuildingList::End(); ++it)
    {
        ++index;
        Box box = (*it)->GetBoundaries();
        outFile << "set object " << index << " rect from " << box.xMin << "," << box.yMin << " to "
                << box.xMax << "," << box.yMax << " front fs empty " << std::endl;
    }
}

void
PrintGnuplottableUeListToFile(std::string filename)
{
    std::ofstream outFile;
    outFile.open(filename.c_str(), std::ios_base::out | std::ios_base::trunc);
    if (!outFile.is_open())
    {
        NS_LOG_ERROR("Can't open file " << filename);
        return;
    }
    for (NodeList::Iterator it = NodeList::Begin(); it != NodeList::End(); ++it)
    {
        Ptr<Node> node = *it;
        int nDevs = node->GetNDevices();
        for (int j = 0; j < nDevs; j++)
        {
            Ptr<LteUeNetDevice> uedev = node->GetDevice(j)->GetObject<LteUeNetDevice>();
            Ptr<MmWaveUeNetDevice> mmuedev = node->GetDevice(j)->GetObject<MmWaveUeNetDevice>();
            Ptr<McUeNetDevice> mcuedev = node->GetDevice(j)->GetObject<McUeNetDevice>();
            if (uedev)
            {
                Vector pos = node->GetObject<MobilityModel>()->GetPosition();
                outFile << "set label \"" << uedev->GetImsi() << "\" at " << pos.x << "," << pos.y
                        << " left font \"Helvetica,8\" textcolor rgb \"black\" front point pt 1 ps "
                           "0.3 lc rgb \"black\" offset 0,0"
                        << std::endl;
            }
            else if (mmuedev)
            {
                Vector pos = node->GetObject<MobilityModel>()->GetPosition();
                outFile << "set label \"" << mmuedev->GetImsi() << "\" at " << pos.x << "," << pos.y
                        << " left font \"Helvetica,8\" textcolor rgb \"black\" front point pt 1 ps "
                           "0.3 lc rgb \"black\" offset 0,0"
                        << std::endl;
            }
            else if (mcuedev)
            {
                Vector pos = node->GetObject<MobilityModel>()->GetPosition();
                outFile << "set label \"" << mcuedev->GetImsi() << "\" at " << pos.x << "," << pos.y
                        << " left font \"Helvetica,8\" textcolor rgb \"black\" front point pt 1 ps "
                           "0.3 lc rgb \"black\" offset 0,0"
                        << std::endl;
            }
        }
    }
}

void
PrintGnuplottableEnbListToFile(std::string filename)
{
    std::ofstream outFile;
    outFile.open(filename.c_str(), std::ios_base::out | std::ios_base::trunc);
    if (!outFile.is_open())
    {
        NS_LOG_ERROR("Can't open file " << filename);
        return;
    }
    for (NodeList::Iterator it = NodeList::Begin(); it != NodeList::End(); ++it)
    {
        Ptr<Node> node = *it;
        int nDevs = node->GetNDevices();
        for (int j = 0; j < nDevs; j++)
        {
            Ptr<LteEnbNetDevice> enbdev = node->GetDevice(j)->GetObject<LteEnbNetDevice>();
            Ptr<MmWaveEnbNetDevice> mmdev = node->GetDevice(j)->GetObject<MmWaveEnbNetDevice>();
            if (enbdev)
            {
                Vector pos = node->GetObject<MobilityModel>()->GetPosition();
                outFile << "set label \"" << enbdev->GetCellId() << "\" at " << pos.x << ","
                        << pos.y
                        << " left font \"Helvetica,8\" textcolor rgb \"blue\" front  point pt 4 ps "
                           "0.3 lc rgb \"blue\" offset 0,0"
                        << std::endl;
            }
            else if (mmdev)
            {
                Vector pos = node->GetObject<MobilityModel>()->GetPosition();
                outFile << "set label \"" << mmdev->GetCellId() << "\" at " << pos.x << "," << pos.y
                        << " left font \"Helvetica,8\" textcolor rgb \"red\" front  point pt 4 ps "
                           "0.3 lc rgb \"red\" offset 0,0"
                        << std::endl;
            }
        }
    }
}

void
ChangePosition(Ptr<Node> node, Vector vector)
{
    Ptr<MobilityModel> model = node->GetObject<MobilityModel>();
    model->SetPosition(vector);
    NS_LOG_UNCOND("************************--------------------Change "
                  "Position-------------------------------*****************");
}

void
ChangeSpeed(Ptr<Node> n, Vector speed)
{
    n->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(speed);
    NS_LOG_UNCOND("************************--------------------Change "
                  "Speed-------------------------------*****************");
}

void
PrintPosition(Ptr<Node> node)
{
    Ptr<MobilityModel> model = node->GetObject<MobilityModel>();
    NS_LOG_UNCOND("Position +****************************** " << model->GetPosition() << " at time "
                                                              << Simulator::Now().GetSeconds());
}

void
PrintLostUdpPackets(Ptr<UdpServer> app, std::string fileName)
{
    std::ofstream logFile(fileName.c_str(), std::ofstream::app);
    logFile << Simulator::Now().GetSeconds() << " " << app->GetLost() << std::endl;
    logFile.close();
    Simulator::Schedule(MilliSeconds(20), &PrintLostUdpPackets, app, fileName);
}

bool
AreOverlapping(Box a, Box b)
{
    return !((a.xMin > b.xMax) || (b.xMin > a.xMax) || (a.yMin > b.yMax) || (b.yMin > a.yMax));
}

bool
OverlapWithAnyPrevious(Box box, std::list<Box> m_previousBlocks)
{
    for (std::list<Box>::iterator it = m_previousBlocks.begin(); it != m_previousBlocks.end(); ++it)
    {
        if (AreOverlapping(*it, box))
        {
            return true;
        }
    }
    return false;
}

std::pair<Box, std::list<Box>>
GenerateBuildingBounds(double xArea,
                       double yArea,
                       double maxBuildSize,
                       std::list<Box> m_previousBlocks)
{
    Ptr<UniformRandomVariable> xMinBuilding = CreateObject<UniformRandomVariable>();
    xMinBuilding->SetAttribute("Min", DoubleValue(30));
    xMinBuilding->SetAttribute("Max", DoubleValue(xArea));

    // NS_LOG_UNCOND("min " << 0 << " max " << xArea);

    Ptr<UniformRandomVariable> yMinBuilding = CreateObject<UniformRandomVariable>();
    yMinBuilding->SetAttribute("Min", DoubleValue(0));
    yMinBuilding->SetAttribute("Max", DoubleValue(yArea));

    // NS_LOG_UNCOND("min " << 0 << " max " << yArea);

    Box box;
    uint32_t attempt = 0;
    do
    {
        NS_ASSERT_MSG(attempt < 100,
                      "Too many failed attempts to position non-overlapping buildings. Maybe area "
                      "too small or too many buildings?");
        box.xMin = xMinBuilding->GetValue();

        Ptr<UniformRandomVariable> xMaxBuilding = CreateObject<UniformRandomVariable>();
        xMaxBuilding->SetAttribute("Min", DoubleValue(box.xMin));
        xMaxBuilding->SetAttribute("Max", DoubleValue(box.xMin + maxBuildSize));
        box.xMax = xMaxBuilding->GetValue();

        box.yMin = yMinBuilding->GetValue();

        Ptr<UniformRandomVariable> yMaxBuilding = CreateObject<UniformRandomVariable>();
        yMaxBuilding->SetAttribute("Min", DoubleValue(box.yMin));
        yMaxBuilding->SetAttribute("Max", DoubleValue(box.yMin + maxBuildSize));
        box.yMax = yMaxBuilding->GetValue();

        ++attempt;
    } while (OverlapWithAnyPrevious(box, m_previousBlocks));

    NS_LOG_UNCOND("Building in coordinates (" << box.xMin << " , " << box.yMin << ") and ("
                                              << box.xMax << " , " << box.yMax
                                              << ") accepted after " << attempt << " attempts");
    m_previousBlocks.push_back(box);
    std::pair<Box, std::list<Box>> pairReturn = std::make_pair(box, m_previousBlocks);
    return pairReturn;
}

static ns3::GlobalValue g_mmw1DistFromMainStreet(
    "mmw1Dist",
    "Distance from the main street of the first MmWaveEnb",
    ns3::UintegerValue(50),
    ns3::MakeUintegerChecker<uint32_t>());
static ns3::GlobalValue g_mmw2DistFromMainStreet(
    "mmw2Dist",
    "Distance from the main street of the second MmWaveEnb",
    ns3::UintegerValue(50),
    ns3::MakeUintegerChecker<uint32_t>());
static ns3::GlobalValue g_mmw3DistFromMainStreet(
    "mmw3Dist",
    "Distance from the main street of the third MmWaveEnb",
    ns3::UintegerValue(110),
    ns3::MakeUintegerChecker<uint32_t>());
static ns3::GlobalValue g_mmWaveDistance("mmWaveDist",
                                         "Distance between MmWave eNB 1 and 2",
                                         ns3::UintegerValue(200),
                                         ns3::MakeUintegerChecker<uint32_t>());
static ns3::GlobalValue g_numBuildingsBetweenMmWaveEnb(
    "numBlocks",
    "Number of buildings between MmWave eNB 1 and 2",
    ns3::UintegerValue(8),
    ns3::MakeUintegerChecker<uint32_t>());
static ns3::GlobalValue g_interPckInterval("interPckInterval",
                                           "Interarrival time of UDP packets (us)",
                                           ns3::UintegerValue(20),
                                           ns3::MakeUintegerChecker<uint32_t>());
static ns3::GlobalValue g_bufferSize("bufferSize",
                                     "RLC tx buffer size (MB)",
                                     ns3::UintegerValue(20),
                                     ns3::MakeUintegerChecker<uint32_t>());
static ns3::GlobalValue g_x2Latency("x2Latency",
                                    "Latency on X2 interface (us)",
                                    ns3::DoubleValue(500),
                                    ns3::MakeDoubleChecker<double>());
static ns3::GlobalValue g_mmeLatency("mmeLatency",
                                     "Latency on MME interface (us)",
                                     ns3::DoubleValue(10000),
                                     ns3::MakeDoubleChecker<double>());
static ns3::GlobalValue g_mobileUeSpeed("mobileSpeed",
                                        "The speed of the UE (m/s)",
                                        ns3::DoubleValue(2),
                                        ns3::MakeDoubleChecker<double>());
static ns3::GlobalValue g_rlcAmEnabled("rlcAmEnabled",
                                       "If true, use RLC AM, else use RLC UM",
                                       ns3::BooleanValue(true),
                                       ns3::MakeBooleanChecker());
static ns3::GlobalValue g_maxXAxis(
    "maxXAxis",
    "The maximum X coordinate for the area in which to deploy the buildings",
    ns3::DoubleValue(150),
    ns3::MakeDoubleChecker<double>());
static ns3::GlobalValue g_maxYAxis(
    "maxYAxis",
    "The maximum Y coordinate for the area in which to deploy the buildings",
    ns3::DoubleValue(40),
    ns3::MakeDoubleChecker<double>());
static ns3::GlobalValue g_outPath("outPath",
                                  "The path of output log files",
                                  ns3::StringValue("./"),
                                  ns3::MakeStringChecker());
static ns3::GlobalValue g_noiseAndFilter(
    "noiseAndFilter",
    "If true, use noisy SINR samples, filtered. If false, just use the SINR measure",
    ns3::BooleanValue(false),
    ns3::MakeBooleanChecker());
static ns3::GlobalValue g_handoverMode("handoverMode",
                                       "Handover mode",
                                       ns3::UintegerValue(3),
                                       ns3::MakeUintegerChecker<uint8_t>());
static ns3::GlobalValue g_reportTablePeriodicity("reportTablePeriodicity",
                                                 "Periodicity of RTs",
                                                 ns3::UintegerValue(1600),
                                                 ns3::MakeUintegerChecker<uint32_t>());
static ns3::GlobalValue g_outageThreshold("outageTh",
                                          "Outage threshold",
                                          ns3::DoubleValue(-5),
                                          ns3::MakeDoubleChecker<double>());
static ns3::GlobalValue g_lteUplink("lteUplink",
                                    "If true, always use LTE for uplink signalling",
                                    ns3::BooleanValue(false),
                                    ns3::MakeBooleanChecker());

int
main(int argc, char* argv[])
{
    LogComponentEnableAll (LOG_PREFIX_TIME);
    LogComponentEnableAll (LOG_PREFIX_FUNC);
    LogComponentEnableAll (LOG_PREFIX_NODE);
    LogComponentEnable ("LteEnbRrc", LOG_LEVEL_DEBUG);
    LogComponentEnable ("MmWaveHelper", LOG_LEVEL_DEBUG);
    // LogComponentEnable ("UdpSocket", LOG_LEVEL_LOGIC);
    // LogComponentEnable ("UdpSocketImpl", LOG_LEVEL_LOGIC);
    // LogComponentEnable ("UdpL4Protocol", LOG_LEVEL_LOGIC);
    // LogComponentEnableAll (LOG_LEVEL_DEBUG);
    // LogComponentEnable ("MmWaveEnbPhy", LOG_LEVEL_DEBUG);
    // LogComponentEnable ("ThreeGppPropagationLossModel", LOG_LEVEL_DEBUG);
    // LogComponentEnable ("ThreeGppChannelModel", LOG_LEVEL_DEBUG);
    // LogComponentEnable ("BuildingsChannelConditionModel", LOG_LEVEL_DEBUG);
    // LogComponentEnable ("MmWavePointToPointEpcHelper", LOG_LEVEL_DEBUG);
    LogComponentEnable ("MpquicTwoPlmn", LOG_LEVEL_ALL);

    bool harqEnabled = true;
    bool fixedTti = false;
    RngSeedManager::SetSeed(RSEED);
    std::list<Box> m_previousBlocks;

    // Command line arguments
    CommandLine cmd;
    cmd.Parse(argc, argv);

    UintegerValue uintegerValue;
    BooleanValue booleanValue;
    StringValue stringValue;
    DoubleValue doubleValue;
    // EnumValue enumValue;
    GlobalValue::GetValueByName("numBlocks", uintegerValue);
    uint32_t numBlocks = uintegerValue.Get();
    GlobalValue::GetValueByName("maxXAxis", doubleValue);
    double maxXAxis = doubleValue.Get();
    GlobalValue::GetValueByName("maxYAxis", doubleValue);
    double maxYAxis = doubleValue.Get();

    double ueInitialPosition = 90;
    double ueFinalPosition = 110;

    // Variables for the RT
    int windowForTransient = 150; // number of samples for the vector to use in the filter
    GlobalValue::GetValueByName("reportTablePeriodicity", uintegerValue);
    int ReportTablePeriodicity = (int)uintegerValue.Get(); // in microseconds
    if (ReportTablePeriodicity == 1600)
    {
        windowForTransient = 150;
    }
    else if (ReportTablePeriodicity == 25600)
    {
        windowForTransient = 50;
    }
    else if (ReportTablePeriodicity == 12800)
    {
        windowForTransient = 100;
    }
    else
    {
        NS_ASSERT_MSG(false, "Unrecognized");
    }

    int vectorTransient = windowForTransient * ReportTablePeriodicity;

    // params for RT, filter, HO mode
    GlobalValue::GetValueByName("noiseAndFilter", booleanValue);
    bool noiseAndFilter = booleanValue.Get();
    GlobalValue::GetValueByName("handoverMode", uintegerValue);
    uint8_t hoMode = uintegerValue.Get();
    GlobalValue::GetValueByName("outageTh", doubleValue);
    double outageTh = doubleValue.Get();

    GlobalValue::GetValueByName("rlcAmEnabled", booleanValue);
    bool rlcAmEnabled = booleanValue.Get();
    GlobalValue::GetValueByName("bufferSize", uintegerValue);
    uint32_t bufferSize = uintegerValue.Get();
    GlobalValue::GetValueByName("interPckInterval", uintegerValue);
    uint32_t interPacketInterval = uintegerValue.Get();
    GlobalValue::GetValueByName("x2Latency", doubleValue);
    double x2Latency = doubleValue.Get();
    GlobalValue::GetValueByName("mmeLatency", doubleValue);
    double mmeLatency = doubleValue.Get();
    GlobalValue::GetValueByName("mobileSpeed", doubleValue);
    double ueSpeed = doubleValue.Get();

    double transientDuration = double(vectorTransient) / 1000000;
    double simTime =
        transientDuration + ((double)ueFinalPosition - (double)ueInitialPosition) / ueSpeed + 1;

    NS_LOG_UNCOND("rlcAmEnabled " << rlcAmEnabled << " bufferSize " << bufferSize
                                  << " interPacketInterval " << interPacketInterval << " x2Latency "
                                  << x2Latency << " mmeLatency " << mmeLatency << " mobileSpeed "
                                  << ueSpeed);

    GlobalValue::GetValueByName("outPath", stringValue);
    std::string path = stringValue.Get();
    std::string mmWaveOutName = "MmWaveSwitchStats";
    std::string lteOutName = "LteSwitchStats";
    std::string dlRlcOutName = "DlRlcStats";
    std::string dlPdcpOutName = "DlPdcpStats";
    std::string ulRlcOutName = "UlRlcStats";
    std::string ulPdcpOutName = "UlPdcpStats";
    std::string ueHandoverStartOutName = "UeHandoverStartStats";
    std::string enbHandoverStartOutName = "EnbHandoverStartStats";
    std::string ueHandoverEndOutName = "UeHandoverEndStats";
    std::string enbHandoverEndOutName = "EnbHandoverEndStats";
    std::string cellIdInTimeOutName = "CellIdStats";
    std::string cellIdInTimeHandoverOutName = "CellIdStatsHandover";
    std::string mmWaveSinrOutputFilename = "MmWaveSinrTime";
    std::string x2statOutputFilename = "X2Stats";
    std::string udpSentFilename = "UdpSent";
    std::string udpReceivedFilename = "UdpReceived";
    std::string extension = ".txt";
    std::string version;
    version = "mc";
    Config::SetDefault("ns3::MmWaveUeMac::UpdateUeSinrEstimatePeriod", DoubleValue(0));

    // get current time
    time_t rawtime;
    struct tm* timeinfo;
    char buffer[80];
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(buffer, 80, "%d_%m_%Y_%I_%M_%S", timeinfo);
    std::string time_str(buffer);

    Config::SetDefault("ns3::MmWaveHelper::RlcAmEnabled", BooleanValue(rlcAmEnabled));
    Config::SetDefault("ns3::MmWaveHelper::HarqEnabled", BooleanValue(harqEnabled));
    Config::SetDefault("ns3::MmWaveFlexTtiMacScheduler::HarqEnabled", BooleanValue(harqEnabled));
    Config::SetDefault("ns3::MmWaveFlexTtiMaxWeightMacScheduler::HarqEnabled",
                       BooleanValue(harqEnabled));
    Config::SetDefault("ns3::MmWaveFlexTtiMaxWeightMacScheduler::FixedTti", BooleanValue(fixedTti));
    Config::SetDefault("ns3::MmWaveFlexTtiMaxWeightMacScheduler::SymPerSlot", UintegerValue(6));
    Config::SetDefault("ns3::MmWavePhyMacCommon::TbDecodeLatency", UintegerValue(200.0));
    Config::SetDefault("ns3::MmWavePhyMacCommon::NumHarqProcess", UintegerValue(100));
    Config::SetDefault("ns3::ThreeGppChannelModel::UpdatePeriod", TimeValue(MilliSeconds(100.0)));
    Config::SetDefault("ns3::LteEnbRrc::SystemInformationPeriodicity",
                       TimeValue(MilliSeconds(5.0)));
    Config::SetDefault("ns3::LteRlcAm::ReportBufferStatusTimer", TimeValue(MicroSeconds(100.0)));
    Config::SetDefault("ns3::LteRlcUmLowLat::ReportBufferStatusTimer",
                       TimeValue(MicroSeconds(100.0)));
    Config::SetDefault("ns3::LteEnbRrc::SrsPeriodicity", UintegerValue(320));
    Config::SetDefault("ns3::LteEnbRrc::FirstSibTime", UintegerValue(2));
    Config::SetDefault("ns3::MmWavePointToPointEpcHelper::X2LinkDelay",
                       TimeValue(MicroSeconds(x2Latency)));
    Config::SetDefault("ns3::MmWavePointToPointEpcHelper::X2LinkDataRate",
                       DataRateValue(DataRate("1000Gb/s")));
    Config::SetDefault("ns3::MmWavePointToPointEpcHelper::X2LinkMtu", UintegerValue(10000));
    Config::SetDefault("ns3::MmWavePointToPointEpcHelper::S1uLinkDelay",
                       TimeValue(MicroSeconds(1000)));
    Config::SetDefault("ns3::MmWavePointToPointEpcHelper::S1apLinkDelay",
                       TimeValue(MicroSeconds(mmeLatency)));
    Config::SetDefault("ns3::LteRlcUm::MaxTxBufferSize", UintegerValue(bufferSize * 1024 * 1024));
    Config::SetDefault("ns3::LteRlcUmLowLat::MaxTxBufferSize",
                       UintegerValue(bufferSize * 1024 * 1024));
    Config::SetDefault("ns3::LteRlcAm::StatusProhibitTimer", TimeValue(MilliSeconds(10.0)));
    Config::SetDefault("ns3::LteRlcAm::MaxTxBufferSize", UintegerValue(bufferSize * 1024 * 1024));

    // handover and RT related params
    switch (hoMode)
    {
    case 1:
        Config::SetDefault("ns3::LteEnbRrc::SecondaryCellHandoverMode",
                           EnumValue(LteEnbRrc::THRESHOLD));
        break;
    case 2:
        Config::SetDefault("ns3::LteEnbRrc::SecondaryCellHandoverMode",
                           EnumValue(LteEnbRrc::FIXED_TTT));
        break;
    case 3:
        Config::SetDefault("ns3::LteEnbRrc::SecondaryCellHandoverMode",
                           EnumValue(LteEnbRrc::DYNAMIC_TTT));
        break;
    }

    Config::SetDefault("ns3::LteEnbRrc::FixedTttValue", UintegerValue(150));
    Config::SetDefault("ns3::LteEnbRrc::CrtPeriod", IntegerValue(ReportTablePeriodicity));
    Config::SetDefault("ns3::LteEnbRrc::OutageThreshold", DoubleValue(outageTh));
    Config::SetDefault("ns3::MmWaveEnbPhy::UpdateSinrEstimatePeriod",
                       IntegerValue(ReportTablePeriodicity));
    Config::SetDefault("ns3::MmWaveEnbPhy::Transient", IntegerValue(vectorTransient));
    Config::SetDefault("ns3::MmWaveEnbPhy::NoiseAndFilter", BooleanValue(noiseAndFilter));

    // set the type of RRC to use, i.e., ideal or real
    // by setting the following two attributes to true, the simulation will use
    // the ideal paradigm, meaning no packets are sent. in fact, only the callbacks are triggered
    Config::SetDefault("ns3::MmWaveHelper::UseIdealRrc", BooleanValue(true));

    GlobalValue::GetValueByName("lteUplink", booleanValue);
    bool lteUplink = booleanValue.Get();

    Config::SetDefault("ns3::McUePdcp::LteUplink", BooleanValue(lteUplink));
    std::cout << "Lte uplink " << lteUplink << "\n";

    // settings for the 3GPP the channel
    Config::SetDefault("ns3::ThreeGppChannelModel::UpdatePeriod",
                       TimeValue(MilliSeconds(
                           100))); // interval after which the channel for a moving user is updated,
    Config::SetDefault("ns3::ThreeGppChannelModel::Blockage",
                       BooleanValue(true)); // use blockage or not
    Config::SetDefault("ns3::ThreeGppChannelModel::PortraitMode",
                       BooleanValue(true)); // use blockage model with UT in portrait mode
    Config::SetDefault("ns3::ThreeGppChannelModel::NumNonselfBlocking",
                       IntegerValue(4)); // number of non-self blocking obstacles

    // by default, isotropic antennas are used. To use the 3GPP radiation pattern instead, use the
    // <ThreeGppAntennaArrayModel> beware: proper configuration of the bearing and downtilt angles
    // is needed
    Config::SetDefault("ns3::PhasedArrayModel::AntennaElement",
                       PointerValue(CreateObject<IsotropicAntennaModel>()));

    // MPQUIC default config
    int ccType = QuicSocketBase::QuicNewReno;
    TypeId ccTypeId = QuicCongestionOps::GetTypeId();
    int schedulerType = MpQuicScheduler::ROUND_ROBIN;
    int mrate = 52428800;
    int mselect = 3;
    uint32_t maxBytes = 52428800;

    Config::SetDefault ("ns3::QuicSocketBase::SocketSndBufSize",UintegerValue (40000000));
    Config::SetDefault ("ns3::QuicStreamBase::StreamSndBufSize",UintegerValue (40000000));
    Config::SetDefault ("ns3::QuicSocketBase::SocketRcvBufSize",UintegerValue (40000000));
    Config::SetDefault ("ns3::QuicStreamBase::StreamRcvBufSize",UintegerValue (40000000));

    Config::SetDefault ("ns3::QuicSocketBase::EnableMultipath",BooleanValue(false));
    Config::SetDefault ("ns3::QuicSocketBase::CcType",IntegerValue(ccType));
    Config::SetDefault ("ns3::QuicL4Protocol::SocketType",TypeIdValue (ccTypeId));
    Config::SetDefault ("ns3::MpQuicScheduler::SchedulerType", IntegerValue(schedulerType));   
    // Config::SetDefault ("ns3::MpQuicScheduler::BlestVar", UintegerValue(bVar));   
    // Config::SetDefault ("ns3::MpQuicScheduler::BlestLambda", UintegerValue(bLambda));     
    Config::SetDefault ("ns3::MpQuicScheduler::MabRate", UintegerValue(mrate)); 
    Config::SetDefault ("ns3::MpQuicScheduler::Select", UintegerValue(mselect)); 
    // eof config

    NS_LOG_UNCOND("config phase accomplished");

    // MPQUIC
    // create a single RemoteHost and a uenode
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ipv4AddressHelper ipv4h;
    QuicHelper quicInternet;
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);
    quicInternet.InstallQuic(remoteHostContainer);
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    NodeContainer ueNodes;
    ueNodes.Create(1);
    quicInternet.InstallQuic(ueNodes);
    Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting(ueNodes.Get(0)->GetObject<Ipv4>());

    Ptr<MmWaveHelper> mmwaveHelper = CreateObject<MmWaveHelper>();
    mmwaveHelper->SetPathlossModelType("ns3::ThreeGppUmiStreetCanyonPropagationLossModel");
    mmwaveHelper->SetChannelConditionModelType("ns3::BuildingsChannelConditionModel");

    // set the number of antennas for both UEs and eNBs
    mmwaveHelper->SetUePhasedArrayModelAttribute("NumColumns", UintegerValue(4));
    mmwaveHelper->SetUePhasedArrayModelAttribute("NumRows", UintegerValue(4));
    mmwaveHelper->SetEnbPhasedArrayModelAttribute("NumColumns", UintegerValue(8));
    mmwaveHelper->SetEnbPhasedArrayModelAttribute("NumRows", UintegerValue(8));

    Ptr<MmWavePointToPointEpcHelper> epcHelper = CreateObject<MmWavePointToPointEpcHelper>();
    mmwaveHelper->SetEpcHelper(epcHelper);
    mmwaveHelper->SetHarqEnabled(harqEnabled);
    mmwaveHelper->Initialize();

    NodeContainer mmWaveEnbNodes;
    NodeContainer lteEnbNodes;
    NodeContainer allEnbNodes;
    mmWaveEnbNodes.Create(2);
    lteEnbNodes.Create(1);
    allEnbNodes.Add(lteEnbNodes);
    allEnbNodes.Add(mmWaveEnbNodes);
    // Positions
    Vector mmw1Position = Vector(50, 70, 3);
    Vector mmw2Position = Vector(150, 70, 3);

    NS_LOG_UNCOND("first mmwave instantited");

#ifdef TWO_PLMN
    // MPQUIC: duplicate on a second PLMN
    Ptr<MmWaveHelper> mmwaveHelper_dup = CreateObject<MmWaveHelper>();
    mmwaveHelper_dup->SetPathlossModelType("ns3::ThreeGppUmiStreetCanyonPropagationLossModel");
    mmwaveHelper_dup->SetChannelConditionModelType("ns3::BuildingsChannelConditionModel");
    mmwaveHelper_dup->SetUePhasedArrayModelAttribute("NumColumns", UintegerValue(4));
    mmwaveHelper_dup->SetUePhasedArrayModelAttribute("NumRows", UintegerValue(4));
    mmwaveHelper_dup->SetEnbPhasedArrayModelAttribute("NumColumns", UintegerValue(8));
    mmwaveHelper_dup->SetEnbPhasedArrayModelAttribute("NumRows", UintegerValue(8));
    Ptr<MmWavePointToPointEpcHelper> epcHelper_dup = CreateObject<MmWavePointToPointEpcHelper>(
                                    "8.0.0.0",
                                    "7778:f00d::",
                                    "15.0.0.0",
                                    "16.0.0.0",
                                    "17.0.0.0"
                                    );
    mmwaveHelper_dup->SetEpcHelper(epcHelper_dup);
    mmwaveHelper_dup->SetHarqEnabled(harqEnabled);
    // mmwaveHelper_dup->SetAttribute("UeBaseAddress", StringValue("8.0.0.0"));
    // mmwaveHelper_dup->SetAttribute("X2BaseAddress", StringValue("15.0.0.0"));
    // mmwaveHelper_dup->SetAttribute("S1apBaseAddress", StringValue("16.0.0.0"));
    // mmwaveHelper_dup->SetAttribute("S1uBaseAddress", StringValue("17.0.0.0"));
    mmwaveHelper_dup->Initialize();

    NodeContainer mmWaveEnbNodes_dup;
    NodeContainer lteEnbNodes_dup;
    NodeContainer allEnbNodes_dup;
    mmWaveEnbNodes_dup.Create(2);
    lteEnbNodes_dup.Create(1);
    allEnbNodes_dup.Add(lteEnbNodes_dup);
    allEnbNodes_dup.Add(mmWaveEnbNodes_dup);
    // Positions
    Vector mmw1Position_dup = Vector(50, 60, 3);
    Vector mmw2Position_dup = Vector(150, 60, 3);

    NS_LOG_UNCOND("second mmwave instantited");

#endif

    ConfigStore inputConfig;
    inputConfig.ConfigureDefaults();

    // parse again so you can override default values from the command line
    cmd.Parse(argc, argv);

    // Get SGW/PGW
    Ptr<Node> pgw = epcHelper->GetPgwNode();
    // Create the Internet by connecting remoteHost to pgw. Setup routing too
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
    p2ph.SetDeviceAttribute("Mtu", UintegerValue(2500));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.010)));
    NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHost);
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);
    // interface 0 is localhost, 1 is the p2p device
    Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress(1);
    NS_LOG_UNCOND("remote host connect with first pgw");

#ifdef TWO_PLMN
    // MPQUIC: duplicate pgw
    Ptr<Node> pgw_dup = epcHelper_dup->GetPgwNode();
    // Create the Internet by connecting remoteHost to pgw. Setup routing too
    PointToPointHelper p2ph_dup;
    p2ph_dup.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
    p2ph_dup.SetDeviceAttribute("Mtu", UintegerValue(2500));
    p2ph_dup.SetChannelAttribute("Delay", TimeValue(Seconds(0.010)));
    NetDeviceContainer internetDevices_dup = p2ph_dup.Install(pgw_dup, remoteHost);
    ipv4h.SetBase("2.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces_dup = ipv4h.Assign(internetDevices_dup);
    // interface 0 is localhost, 1 is the p2p device
    Ipv4Address remoteHostAddr_dup = internetIpIfaces_dup.GetAddress(1);
    NS_LOG_UNCOND("remote host connect with second pgw");
#endif

{    // building
    std::vector<Ptr<Building>> buildingVector;

    double maxBuildingSize = 20;

    for (uint32_t buildingIndex = 0; buildingIndex < numBlocks; buildingIndex++)
    {
        Ptr<Building> building;
        building = Create<Building>();
        /* returns a vecotr where:
         * position [0]: coordinates for x min
         * position [1]: coordinates for x max
         * position [2]: coordinates for y min
         * position [3]: coordinates for y max
         */
        std::pair<Box, std::list<Box>> pairBuildings =
            GenerateBuildingBounds(maxXAxis, maxYAxis, maxBuildingSize, m_previousBlocks);
        m_previousBlocks = std::get<1>(pairBuildings);
        Box box = std::get<0>(pairBuildings);
        Ptr<UniformRandomVariable> randomBuildingZ = CreateObject<UniformRandomVariable>();
        randomBuildingZ->SetAttribute("Min", DoubleValue(1.6));
        randomBuildingZ->SetAttribute("Max", DoubleValue(40));
        double buildingHeight = randomBuildingZ->GetValue();

        building->SetBoundaries(Box(box.xMin, box.xMax, box.yMin, box.yMax, 0.0, buildingHeight));
        buildingVector.push_back(building);
    }
    // eof building
}

    // Install Mobility Model
    Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator>();
    enbPositionAlloc->Add(mmw1Position); // LTE BS, out of area where buildings are deployed
    enbPositionAlloc->Add(mmw1Position);
    enbPositionAlloc->Add(mmw2Position);
    MobilityHelper enbmobility;
    enbmobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    enbmobility.SetPositionAllocator(enbPositionAlloc);
    enbmobility.Install(allEnbNodes);
    BuildingsHelper::Install(allEnbNodes);

    MobilityHelper uemobility;
    Ptr<ListPositionAllocator> uePositionAlloc = CreateObject<ListPositionAllocator>();
    uePositionAlloc->Add(Vector(ueInitialPosition, -5, 1.6));
    uemobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    uemobility.SetPositionAllocator(uePositionAlloc);
    uemobility.Install(ueNodes);
    BuildingsHelper::Install(ueNodes);
    ueNodes.Get(0)->GetObject<MobilityModel>()->SetPosition(Vector(ueInitialPosition, -5, 1.6));
    ueNodes.Get(0)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(ueSpeed, 0, 0));

#ifdef TWO_PLMN
    // MPQUIC: duplicate mobility model
    // Install Mobility Model
    Ptr<ListPositionAllocator> enbPositionAlloc_dup = CreateObject<ListPositionAllocator>();
    enbPositionAlloc_dup->Add(mmw1Position_dup); // LTE BS, out of area where buildings are deployed
    enbPositionAlloc_dup->Add(mmw1Position_dup);
    enbPositionAlloc_dup->Add(mmw2Position_dup);
    MobilityHelper enbmobility_dup;
    enbmobility_dup.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    enbmobility_dup.SetPositionAllocator(enbPositionAlloc_dup);
    enbmobility_dup.Install(allEnbNodes_dup);
    BuildingsHelper::Install(allEnbNodes_dup); 
#endif


    // Install mmWave, lte, mc Devices to the nodes
    NetDeviceContainer lteEnbDevs = mmwaveHelper->InstallLteEnbDevice(lteEnbNodes, 10);
    NetDeviceContainer mmWaveEnbDevs = mmwaveHelper->InstallEnbDevice(mmWaveEnbNodes);
    NetDeviceContainer mcUeDevs = mmwaveHelper->InstallMcUeDevice(ueNodes, 10);

    // Install the IP stack on the UEs // MPQUIC
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(mcUeDevs));
    NS_LOG_UNCOND("---\n" << "ue_iface ip:" << ueIpIface.GetAddress(0) << " ue id:" << ueNodes.Get(0)->GetId()
                            << " ue imsi:" << mcUeDevs.Get(0)->GetObject<McUeNetDevice>()->GetImsi());
    NS_LOG_UNCOND("host ip:" << remoteHostAddr << " host id:" << remoteHost->GetId());
    NS_LOG_UNCOND("pgw ip:" << internetIpIfaces.GetAddress(0) << " pgw id:" << pgw->GetId());
    NS_LOG_UNCOND("mme id:" << epcHelper->GetMmeNode()->GetId());
    NS_LOG_UNCOND("plmn gateway:" << epcHelper->GetUeDefaultGatewayAddress() << "\n---");
    // Assign IP address to UEs, and install applications

    // Add X2 interfaces
    mmwaveHelper->AddX2Interface(lteEnbNodes, mmWaveEnbNodes);
    // Manual attachment
    mmwaveHelper->AttachToClosestEnb(mcUeDevs, mmWaveEnbDevs, lteEnbDevs);

#ifdef TWO_PLMN
    // MPQUIC: connect dup ue, bs with epc
    // Install mmWave, lte, mc Devices to the nodes
    NetDeviceContainer lteEnbDevs_dup = mmwaveHelper_dup->InstallLteEnbDevice(lteEnbNodes_dup, 20);
    NetDeviceContainer mmWaveEnbDevs_dup = mmwaveHelper_dup->InstallEnbDevice(mmWaveEnbNodes_dup);
    NetDeviceContainer mcUeDevs_dup = mmwaveHelper_dup->InstallMcUeDevice(ueNodes, 20); 

    Ipv4InterfaceContainer ueIpIface_dup = epcHelper_dup->AssignUeIpv4Address(NetDeviceContainer(mcUeDevs_dup));

    NS_LOG_UNCOND("---\n" << "ue_iface_dup ip:" << ueIpIface_dup.GetAddress(0) << " ue id:" << ueNodes.Get(0)->GetId()
                            << " ue imsi:" << mcUeDevs_dup.Get(0)->GetObject<McUeNetDevice>()->GetImsi());
    NS_LOG_UNCOND("host_dup ip:" << remoteHostAddr_dup << " host id:" << remoteHost->GetId());
    NS_LOG_UNCOND("pgw_dup ip:" << internetIpIfaces_dup.GetAddress(0) << " pgw_dup id:" << pgw_dup->GetId());
    NS_LOG_UNCOND("mme_dup id:" << epcHelper_dup->GetMmeNode()->GetId());
    NS_LOG_UNCOND("plmn_dup gateway:" << epcHelper_dup->GetUeDefaultGatewayAddress() << "\n---");
    // Assign IP address to UEs, and install applications
    // Set the default gateway for the UE

    mmwaveHelper_dup->AddX2Interface(lteEnbNodes_dup, mmWaveEnbNodes_dup);
    mmwaveHelper_dup->AttachToClosestEnb(mcUeDevs_dup, mmWaveEnbDevs_dup, lteEnbDevs_dup);
    //
    NS_LOG_UNCOND("ue and bs all attached");
#endif

    
    // MPQUIC routing
    // ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1); 
    ueStaticRouting->AddHostRouteTo(remoteHostAddr, epcHelper->GetUeDefaultGatewayAddress(),  1);
    remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

#ifdef TWO_PLMN
    // ueStaticRouting->SetDefaultRoute(epcHelper_dup->GetUeDefaultGatewayAddress(), 1); 
    ueStaticRouting->AddHostRouteTo(remoteHostAddr_dup, epcHelper_dup->GetUeDefaultGatewayAddress(), 2);
    remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("8.0.0.0"), Ipv4Mask("255.0.0.0"), 2);
#endif

    // NS_LOG_UNCOND("BEFORE");
    // Ipv4GlobalRoutingHelper::PopulateRoutingTables (); // this is buggy
    // NS_LOG_UNCOND("AFTER");


    // print cell id
    // Ptr<MmWaveEnbNetDevice> mmdev;
    // mmdev = mmWaveEnbNodes.Get(0)->GetDevice(0)->GetObject<MmWaveEnbNetDevice>();
    // NS_LOG_UNCOND("mmwave 0 cell id: " << mmdev->GetCellId());
    // mmdev = mmWaveEnbNodes_dup.Get(0)->GetDevice(0)->GetObject<MmWaveEnbNetDevice>();
    // NS_LOG_UNCOND("mmwave_dup 0 cell id: " << mmdev->GetCellId());
    NS_LOG_UNCOND("---\n" << "PLMN1 bs nodes: "    << allEnbNodes.Get(0)->GetId() << " " 
                                        << allEnbNodes.Get(1)->GetId() << " " 
                                        << allEnbNodes.Get(2)->GetId() << " ");
    NS_LOG_UNCOND("ue mmwave device id: " << mcUeDevs.Get(0)->GetIfIndex() << "\n---");
#ifdef TWO_PLMN
    NS_LOG_UNCOND("---\n" << "PLMN2 bs nodes: "    << allEnbNodes_dup.Get(0)->GetId() << " " 
                                        << allEnbNodes_dup.Get(1)->GetId() << " " 
                                        << allEnbNodes_dup.Get(2)->GetId() << " " );
    NS_LOG_UNCOND("ue_dup mmwave device id: " << mcUeDevs_dup.Get(0)->GetIfIndex() << "\n---");
#endif
    // APPLICATION PART
    // Install and start applications on UEs and remote host
    uint16_t dlPort = 1234;
    uint16_t ulPort = 2000;
    ApplicationContainer clientApps;
    ApplicationContainer serverApps;

    bool udp_dl = 0;
    bool udp_ul = 0;
    bool quic_up = 1;
    int u = 0; // only the 0th ue 

    if (quic_up) {
        // NS_LOG_UNCOND("try quic upload");
        MpquicBulkSendHelper source("ns3::QuicSocketFactory",
                        InetSocketAddress(remoteHostAddr, ulPort));
        // Set the amount of data to send in bytes.  Zero is unlimited.
        source.SetAttribute ("MaxBytes", UintegerValue (maxBytes));
        clientApps.Add(source.Install(ueNodes.Get(0)));
        // NS_LOG_UNCOND("source inited");

        PacketSinkHelper sink("ns3::QuicSocketFactory",
                                InetSocketAddress(Ipv4Address::GetAny(), ulPort));
        serverApps.Add(sink.Install(remoteHost));
        // NS_LOG_UNCOND("sink inited");
    }
    if (udp_dl)
    {
        UdpServerHelper dlPacketSinkHelper(dlPort);
        dlPacketSinkHelper.SetAttribute("PacketWindowSize", UintegerValue(256));
        serverApps.Add(dlPacketSinkHelper.Install(ueNodes.Get(u)));

        // Simulator::Schedule(MilliSeconds(20), &PrintLostUdpPackets,
        // DynamicCast<UdpServer>(serverApps.Get(serverApps.GetN()-1)), lostFilename);

        UdpClientHelper dlClient(ueIpIface.GetAddress(u), dlPort);
        dlClient.SetAttribute("Interval", TimeValue(MicroSeconds(interPacketInterval)));
        dlClient.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
        clientApps.Add(dlClient.Install(remoteHost));
    }
    if (udp_ul)
    {
        ++ulPort;
        PacketSinkHelper ulPacketSinkHelper("ns3::UdpSocketFactory",
                                            InetSocketAddress(Ipv4Address::GetAny(), ulPort));
        ulPacketSinkHelper.SetAttribute("PacketWindowSize", UintegerValue(256));
        serverApps.Add(ulPacketSinkHelper.Install(remoteHost));
        UdpClientHelper ulClient(remoteHostAddr, ulPort);
        ulClient.SetAttribute("Interval", TimeValue(MicroSeconds(interPacketInterval)));
        ulClient.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
        clientApps.Add(ulClient.Install(ueNodes.Get(u)));
    }


    // Start applications
    NS_LOG_UNCOND("transientDuration " << transientDuration << " simTime " << simTime);
    serverApps.Start(Seconds(transientDuration));
    clientApps.Start(Seconds(transientDuration));
    clientApps.Stop(Seconds(simTime - 1));

    Simulator::Schedule(Seconds(transientDuration),
                        &ChangeSpeed,
                        ueNodes.Get(0),
                        Vector(ueSpeed, 0, 0)); // start UE movement after Seconds(0.5)
    Simulator::Schedule(Seconds(simTime - 1),
                        &ChangeSpeed,
                        ueNodes.Get(0),
                        Vector(0, 0, 0)); // start UE movement after Seconds(0.5)
    // MPQUIC duplicate speedchange

    double numPrints = 0;
    for (int i = 0; i < numPrints; i++)
    {
        Simulator::Schedule(Seconds(i * simTime / numPrints), &PrintPosition, ueNodes.Get(0));
    }

    // mmwaveHelper->EnableTraces();
// #ifdef TWO_PLMN
//     mmwaveHelper_dup->EnableTraces(); 
// #endif
    // how to trace dup??


    // set to true if you want to print the map of buildings, ues and enbs
    bool print = true;
    if (print)
    {
        PrintGnuplottableBuildingListToFile("buildings.txt");
        PrintGnuplottableUeListToFile("ues.txt");
        PrintGnuplottableEnbListToFile("enbs.txt");
    }



    Simulator::Stop(Seconds(simTime));
    NS_LOG_UNCOND("finish all setup");

    // MPQUIC flowmonitor
    AsciiTraceHelper asciiTraceHelper;
    std::ostringstream fileName;
    fileName <<  "./scheduler" << schedulerType << "-rx" << ".txt";
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream (fileName.str ());

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll ();
    ThroughputMonitor(&flowmon, monitor, stream); 
    //
    NS_LOG_UNCOND("START RUNNING SIM");
    Simulator::Run();

    // MPQUIC flowmonitor
    monitor->CheckForLostPackets ();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
        if (i->first == 1 || i->first == 2){

        NS_LOG_INFO("Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")"
        << "\n Last rx Seconds: " << i->second.timeLastRxPacket.GetSeconds()
        << "\n Rx Bytes: " << i->second.rxBytes
        << "\n DelaySum(s): " << i->second.delaySum.GetSeconds()
        << "\n rxPackets: " << i->second.rxPackets);
        }
        
    }
    //

    Simulator::Destroy();
    return 0;
}
