﻿﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using System.Reflection;
using System.Collections;
using Microsoft.Win32;
using System.Runtime.InteropServices;
using System.ComponentModel;
using System.Windows;

namespace EpgTimer
{
    class IniFileHandler
    {
        [DllImport("KERNEL32.DLL")]
        public static extern uint
          GetPrivateProfileString(string lpAppName,
          string lpKeyName, string lpDefault,
          StringBuilder lpReturnedString, uint nSize,
          string lpFileName);

        [DllImport("KERNEL32.DLL",
            EntryPoint = "GetPrivateProfileStringA")]
        public static extern uint
          GetPrivateProfileStringByByteArray(string lpAppName,
          string lpKeyName, string lpDefault,
          byte[] lpReturnedString, uint nSize,
          string lpFileName);

        [DllImport("KERNEL32.DLL")]
        public static extern int
          GetPrivateProfileInt(string lpAppName,
          string lpKeyName, int nDefault, string lpFileName);

        [DllImport("KERNEL32.DLL")]
        public static extern uint WritePrivateProfileString(
          string lpAppName,
          string lpKeyName,
          string lpString,
          string lpFileName);

        public static string
          GetPrivateProfileString(string lpAppName,
          string lpKeyName, string lpDefault, string lpFileName)
        {
            StringBuilder buff = new StringBuilder(512);
            IniFileHandler.GetPrivateProfileString(lpAppName, lpKeyName, lpDefault, buff, 512, lpFileName);
            return buff.ToString();
        }
        
        public static void UpdateSrvProfileIniNW()
        {
            SendIniCopy("EpgTimerSrv.ini");
            SendIniCopy("Common.ini");
            SendIniCopy("EpgDataCap_Bon.ini");

            Settings.UpdateDefRecSetting();
        }

        private static bool SendIniCopy(string iniFileName)
        {
            try
            {
                byte[] binData;
                if (CommonManager.Instance.CtrlCmd.SendFileCopy(iniFileName, out binData) == 1)
                {
                    string filePath = SettingPath.SettingFolderPath;
                    System.IO.Directory.CreateDirectory(filePath);
                    filePath = filePath.TrimEnd('\\') + "\\" + iniFileName;
                    using (System.IO.BinaryWriter w = new System.IO.BinaryWriter(System.IO.File.Create(filePath)))
                    {
                        w.Write(binData);
                        w.Close();
                    }
                    return true;
                }
            }
            catch (Exception ex) { MessageBox.Show(ex.Message + "\r\n" + ex.StackTrace); }
            return false;
        }

    }

    class SettingPath
    {
        private static string IniPath
        {
            get { return (CommonManager.Instance.NWMode == false ? ModulePath : SettingFolderPath); }
        }
        public static string CommonIniPath
        {
            get { return IniPath.TrimEnd('\\') + "\\Common.ini"; }
        }
        public static string TimerSrvIniPath
        {
            get { return IniPath.TrimEnd('\\') + "\\EpgTimerSrv.ini"; }
        }
        public static string EdcbExePath
        {
            get
            {
                string defRecExe = SettingPath.ModulePath.TrimEnd('\\') + "\\EpgDataCap_Bon.exe";
                return IniFileHandler.GetPrivateProfileString("SET", "RecExePath", defRecExe, SettingPath.CommonIniPath);
            }
        }
        public static string EdcbIniPath
        {
            get
            {
                if (CommonManager.Instance.NWMode == false)
                {
                    return EdcbExePath.TrimEnd("exe".ToArray()) + "ini";
                }
                else
                {
                    return IniPath.TrimEnd('\\') + "\\EpgDataCap_Bon.ini";
                }
            }
        }
        public static string DefSettingFolderPath
        {
            get
            {
                return ModulePath.TrimEnd('\\') + "\\Setting" + (CommonManager.Instance.NWMode == false ? "" : "NW");
            }
        }
        public static string SettingFolderPath
        {
            get
            {
                if (CommonManager.Instance.NWMode == false)
                {
                    string path = IniFileHandler.GetPrivateProfileString("SET", "DataSavePath", SettingPath.DefSettingFolderPath, SettingPath.CommonIniPath);
                    return (Path.IsPathRooted(path) ? "" : SettingPath.ModulePath.TrimEnd('\\') + "\\") + path;
                }
                else
                {
                    return SettingPath.DefSettingFolderPath;
                }
            }
        }
        public static string ModulePath
        {
            get
            {
                return Path.GetDirectoryName(Assembly.GetEntryAssembly().Location);
            }
        }
    }

    public class Settings
    {
        private bool useCustomEpgView;
        private List<CustomEpgTabInfo> customEpgTabList;
        private double minHeight;
        private double minimumHeight;
        private double serviceWidth;
        private double scrollSize;
        private string fontName;
        private double fontSize;
        private string fontNameTitle;
        private double fontSizeTitle;
        private bool fontBoldTitle;
        private bool noToolTip;
        private bool noBallonTips;
        private int forceHideBalloonTipSec;
        private bool playDClick;
        private double dragScroll;
        private List<string> contentColorList;
        private List<UInt32> contentCustColorList;
        private List<string> timeColorList;
        private List<UInt32> timeCustColorList;
        private string reserveRectColorNormal;
        private string reserveRectColorNo;
        private string reserveRectColorNoTuner;
        private string reserveRectColorWarning;
        private string titleColor1;
        private string titleColor2;
        private UInt32 titleCustColor1;
        private UInt32 titleCustColor2;
        private string serviceColor;
        private UInt32 serviceCustColor;
        private bool reserveRectBackground;
        private bool epgTitleIndent;
        private bool epgPopup;
        private bool epgGradation;
        private bool epgGradationHeader;
        private string resColumnHead;
        private ListSortDirection resSortDirection;
        private System.Windows.WindowState lastWindowState;
        private double mainWndLeft;
        private double mainWndTop;
        private double mainWndWidth;
        private double mainWndHeight;
        private bool closeMin;
        private bool wakeMin;
        private bool viewButtonShowAsTab;
        private List<string> viewButtonList;
        private List<string> taskMenuList;
        private string cust1BtnName;
        private string cust1BtnCmd;
        private string cust1BtnCmdOpt;
        private string cust2BtnName;
        private string cust2BtnCmd;
        private string cust2BtnCmdOpt;
        private List<string> andKeyList;
        private List<string> notKeyList;
        private bool searchKeyRegExp;
        private bool searchKeyTitleOnly;
        private bool searchKeyAimaiFlag;
        private bool searchKeyNotContent;
        private bool searchKeyNotDate;
        private List<ContentKindInfo> searchKeyContentList;
        private List<DateItem> searchKeyDateItemList;
        private List<Int64> searchKeyServiceList;
        private byte searchKeyFreeCA;
        private byte searchKeyChkRecEnd;
        private UInt16 searchKeyChkRecDay;
        private byte searchKeyChkRecNoService;
        private UInt16 searchKeyChkDurationMin;
        private UInt16 searchKeyChkDurationMax;
        private List<RecPresetItem> recPresetList;
        private string recInfoColumnHead;
        private ListSortDirection recInfoSortDirection;
        private string tvTestExe;
        private string tvTestCmd;
        private bool nwTvMode;
        private bool nwTvModeUDP;
        private bool nwTvModeTCP;
        private string filePlayExe;
        private string filePlayCmd;
        private bool openFolderWithFileDialog;
        private List<IEPGStationInfo> iEpgStationList;
        private MenuSettingData menuSet;
        private string nwServerIP;
        private UInt32 nwServerPort;
        private UInt32 nwWaitPort;
        private string nwMacAdd;
        private bool wakeReconnectNW;
        private bool suspendCloseNW;
        private bool ngAutoEpgLoadNW;
        private Int32 tvTestOpenWait;
        private Int32 tvTestChgBonWait;
        private String listDefFontColor;            //リストのデフォルト文字色
        private List<String> recModeFontColorList;  //録画モードごとの文字色
        private byte resDefColorR;
        private byte resDefColorG;
        private byte resDefColorB;
        private byte resErrColorR;
        private byte resErrColorG;
        private byte resErrColorB;
        private byte resWarColorR;
        private byte resWarColorG;
        private byte resWarColorB;
        private byte resNoColorR;
        private byte resNoColorG;
        private byte resNoColorB;
        private byte recEndDefColorR;
        private byte recEndDefColorG;
        private byte recEndDefColorB;
        private byte recEndErrColorR;
        private byte recEndErrColorG;
        private byte recEndErrColorB;
        private byte recEndWarColorR;
        private byte recEndWarColorG;
        private byte recEndWarColorB;
        private String statResForeColor;            //ステータスの予約色
        private String statRecForeColor;            //ステータスの録画色
        private String statOnAirForeColor;          //ステータスの放送色
        private byte epgTipsBackColorR;
        private byte epgTipsBackColorG;
        private byte epgTipsBackColorB;
        private byte epgTipsForeColorR;
        private byte epgTipsForeColorG;
        private byte epgTipsForeColorB;
        private bool epgInfoSingleClick;
        private byte epgInfoOpenMode;
        private UInt32 execBat;
        private UInt32 suspendChk;
        private UInt32 suspendChkTime;
        private List<ListColumnInfo> reserveListColumn;
        private List<ListColumnInfo> recInfoListColumn;
        private List<ListColumnInfo> autoAddEpgColumn;
        private List<ListColumnInfo> autoAddManualColumn;
        private double searchWndLeft;
        private double searchWndTop;
        private double searchWndWidth;
        private double searchWndHeight;
        private short autoSaveNotifyLog;
        private bool showTray;
        private bool minHide;
        private bool mouseScrollAuto;
        private int noStyle;
        private bool cautionManyChange;
        private int cautionManyNum;
        private int keyDeleteDisplayItemNum;
        private bool displayNotifyEpgChange;
        private int displayNotifyJumpTime;

        public bool UseCustomEpgView
        {
            get { return useCustomEpgView; }
            set { useCustomEpgView = value; }
        }
        public List<CustomEpgTabInfo> CustomEpgTabList
        {
            get { return customEpgTabList; }
            set { customEpgTabList = value; }
        }
        public double MinHeight
        {
            get { return minHeight; }
            set { minHeight = value; }
        }
        public double ServiceWidth
        {
            get { return serviceWidth; }
            set { serviceWidth = value; }
        }
        public double ScrollSize
        {
            get { return scrollSize; }
            set { scrollSize = value; }
        }
        public double MinimumHeight
        {
            get { return minimumHeight; }
            set { minimumHeight = value; }
        }
        public string FontName
        {
            get { return fontName; }
            set { fontName = value; }
        }
        public double FontSize
        {
            get { return fontSize; }
            set { fontSize = value; }
        }
        public string FontNameTitle
        {
            get { return fontNameTitle; }
            set { fontNameTitle = value; }
        }
        public double FontSizeTitle
        {
            get { return fontSizeTitle; }
            set { fontSizeTitle = value; }
        }
        public bool FontBoldTitle
        {
            get { return fontBoldTitle; }
            set { fontBoldTitle = value; }
        }
        public bool NoToolTip
        {
            get { return noToolTip; }
            set { noToolTip = value; }
        }
        public bool NoBallonTips
        {
            get { return noBallonTips; }
            set { noBallonTips = value; }
        }
        public int ForceHideBalloonTipSec
        {
            get { return forceHideBalloonTipSec; }
            set { forceHideBalloonTipSec = value; }
        }
        public bool PlayDClick
        {
            get { return playDClick; }
            set { playDClick = value; }
        }
        public double DragScroll
        {
            get { return dragScroll; }
            set { dragScroll = value; }
        }
        public List<string> ContentColorList
        {
            get { return contentColorList; }
            set { contentColorList = value; }
        }
        public List<UInt32> ContentCustColorList
        {
            get { return contentCustColorList; }
            set { contentCustColorList = value; }
        }
        public List<string> TimeColorList
        {
            get { return timeColorList; }
            set { timeColorList = value; }
        }
        public List<UInt32> TimeCustColorList
        {
            get { return timeCustColorList; }
            set { timeCustColorList = value; }
        }
        public string ReserveRectColorNormal
        {
            get { return reserveRectColorNormal; }
            set { reserveRectColorNormal = value; }
        }
        public string ReserveRectColorNo
        {
            get { return reserveRectColorNo; }
            set { reserveRectColorNo = value; }
        }
        public string ReserveRectColorNoTuner
        {
            get { return reserveRectColorNoTuner; }
            set { reserveRectColorNoTuner = value; }
        }
        public string ReserveRectColorWarning
        {
            get { return reserveRectColorWarning; }
            set { reserveRectColorWarning = value; }
        }
        public bool ReserveRectBackground
        {
            get { return reserveRectBackground; }
            set { reserveRectBackground = value; }
        }
        public string TitleColor1
        {
            get { return titleColor1; }
            set { titleColor1 = value; }
        }
        public string TitleColor2
        {
            get { return titleColor2; }
            set { titleColor2 = value; }
        }
        public UInt32 TitleCustColor1
        {
            get { return titleCustColor1; }
            set { titleCustColor1 = value; }
        }
        public UInt32 TitleCustColor2
        {
            get { return titleCustColor2; }
            set { titleCustColor2 = value; }
        }
        public string ServiceColor
        {
            get { return serviceColor; }
            set { serviceColor = value; }
        }
        public UInt32 ServiceCustColor
        {
            get { return serviceCustColor; }
            set { serviceCustColor = value; }
        }
        public bool EpgTitleIndent
        {
            get { return epgTitleIndent; }
            set { epgTitleIndent = value; }
        }
        public bool EpgPopup
        {
            get { return epgPopup; }
            set { epgPopup = value; }
        }
        public bool EpgGradation
        {
            get { return epgGradation; }
            set { epgGradation = value; }
        }
        public bool EpgGradationHeader
        {
            get { return epgGradationHeader; }
            set { epgGradationHeader = value; }
        }
        public string ResColumnHead
        {
            get { return resColumnHead; }
            set { resColumnHead = value; }
        }
        public ListSortDirection ResSortDirection
        {
            get { return resSortDirection; }
            set { resSortDirection = value; }
        }
        public System.Windows.WindowState LastWindowState
        {
            get { return lastWindowState; }
            set { lastWindowState = value; }
        }
        public double MainWndLeft
        {
            get { return mainWndLeft; }
            set { mainWndLeft = value; }
        }
        public double MainWndTop
        {
            get { return mainWndTop; }
            set { mainWndTop = value; }
        }
        public double MainWndWidth
        {
            get { return mainWndWidth; }
            set { mainWndWidth = value; }
        }
        public double MainWndHeight
        {
            get { return mainWndHeight; }
            set { mainWndHeight = value; }
        }
        public bool CloseMin
        {
            get { return closeMin; }
            set { closeMin = value; }
        }
        public bool WakeMin
        {
            get { return wakeMin; }
            set { wakeMin = value; }
        }
        public bool ViewButtonShowAsTab
        {
            get { return viewButtonShowAsTab; }
            set { viewButtonShowAsTab = value; }
        }
        public List<string> ViewButtonList
        {
            get { return viewButtonList; }
            set { viewButtonList = value; }
        }
        public List<string> TaskMenuList
        {
            get { return taskMenuList; }
            set { taskMenuList = value; }
        }
        public string Cust1BtnName
        {
            get { return cust1BtnName; }
            set { cust1BtnName = value; }
        }
        public string Cust1BtnCmd
        {
            get { return cust1BtnCmd; }
            set { cust1BtnCmd = value; }
        }
        public string Cust1BtnCmdOpt
        {
            get { return cust1BtnCmdOpt; }
            set { cust1BtnCmdOpt = value; }
        }
        public string Cust2BtnName
        {
            get { return cust2BtnName; }
            set { cust2BtnName = value; }
        }
        public string Cust2BtnCmd
        {
            get { return cust2BtnCmd; }
            set { cust2BtnCmd = value; }
        }
        public string Cust2BtnCmdOpt
        {
            get { return cust2BtnCmdOpt; }
            set { cust2BtnCmdOpt = value; }
        }
        public List<string> AndKeyList
        {
            get { return andKeyList; }
            set { andKeyList = value; }
        }
        public List<string> NotKeyList
        {
            get { return notKeyList; }
            set { notKeyList = value; }
        }
        public bool SearchKeyRegExp
        {
            get { return searchKeyRegExp; }
            set { searchKeyRegExp = value; }
        }
        public bool SearchKeyTitleOnly
        {
            get { return searchKeyTitleOnly; }
            set { searchKeyTitleOnly = value; }
        }
        public bool SearchKeyAimaiFlag
        {
            get { return searchKeyAimaiFlag; }
            set { searchKeyAimaiFlag = value; }
        }
        public bool SearchKeyNotContent
        {
            get { return searchKeyNotContent; }
            set { searchKeyNotContent = value; }
        }
        public bool SearchKeyNotDate
        {
            get { return searchKeyNotDate; }
            set { searchKeyNotDate = value; }
        }
        public List<ContentKindInfo> SearchKeyContentList
        {
            get { return searchKeyContentList; }
            set { searchKeyContentList = value; }
        }
        public List<DateItem> SearchKeyDateItemList
        {
            get { return searchKeyDateItemList; }
            set { searchKeyDateItemList = value; }
        }
        public List<Int64> SearchKeyServiceList
        {
            get { return searchKeyServiceList; }
            set { searchKeyServiceList = value; }
        }
        public byte SearchKeyFreeCA
        {
            get { return searchKeyFreeCA; }
            set { searchKeyFreeCA = value; }
        }
        public byte SearchKeyChkRecEnd
        {
            get { return searchKeyChkRecEnd; }
            set { searchKeyChkRecEnd = value; }
        }
        public UInt16 SearchKeyChkRecDay
        {
            get { return searchKeyChkRecDay; }
            set { searchKeyChkRecDay = value; }
        }
        public byte SearchKeyChkRecNoService
        {
            get { return searchKeyChkRecNoService; }
            set { searchKeyChkRecNoService = value; }
        }
        public UInt16 SearchKeyChkDurationMin
        {
            get { return searchKeyChkDurationMin; }
            set { searchKeyChkDurationMin = value; }
        }
        public UInt16 SearchKeyChkDurationMax
        {
            get { return searchKeyChkDurationMax; }
            set { searchKeyChkDurationMax = value; }
        }
        public List<RecPresetItem> RecPresetList
        {
            get { return recPresetList; }
            set { recPresetList = value; }
        }
        public string RecInfoColumnHead
        {
            get { return recInfoColumnHead; }
            set { recInfoColumnHead = value; }
        }
        public ListSortDirection RecInfoSortDirection
        {
            get { return recInfoSortDirection; }
            set { recInfoSortDirection = value; }
        }
        public string TvTestExe
        {
            get { return tvTestExe; }
            set { tvTestExe = value; }
        }
        public string TvTestCmd
        {
            get { return tvTestCmd; }
            set { tvTestCmd = value; }
        }
        public bool NwTvMode
        {
            get { return nwTvMode; }
            set { nwTvMode = value; }
        }
        public bool NwTvModeUDP
        {
            get { return nwTvModeUDP; }
            set { nwTvModeUDP = value; }
        }
        public bool NwTvModeTCP
        {
            get { return nwTvModeTCP; }
            set { nwTvModeTCP = value; }
        }
        public string FilePlayExe
        {
            get { return filePlayExe; }
            set { filePlayExe = value; }
        }
        public string FilePlayCmd
        {
            get { return filePlayCmd; }
            set { filePlayCmd = value; }
        }
        public bool OpenFolderWithFileDialog
        {
            get { return openFolderWithFileDialog; }
            set { openFolderWithFileDialog = value; }
        }
        public List<IEPGStationInfo> IEpgStationList
        {
            get { return iEpgStationList; }
            set { iEpgStationList = value; }
        }
        public MenuSettingData MenuSet
        {
            get { return menuSet; }
            set { menuSet = value; }
        }
        public string NWServerIP
        {
            get { return nwServerIP; }
            set { nwServerIP = value; }
        }
        public UInt32 NWServerPort
        {
            get { return nwServerPort; }
            set { nwServerPort = value; }
        }
        public UInt32 NWWaitPort
        {
            get { return nwWaitPort; }
            set { nwWaitPort = value; }
        }
        public string NWMacAdd
        {
            get { return nwMacAdd; }
            set { nwMacAdd = value; }
        }
        public bool WakeReconnectNW
        {
            get { return wakeReconnectNW; }
            set { wakeReconnectNW = value; }
        }
        public bool SuspendCloseNW
        {
            get { return suspendCloseNW; }
            set { suspendCloseNW = value; }
        }
        public bool NgAutoEpgLoadNW
        {
            get { return ngAutoEpgLoadNW; }
            set { ngAutoEpgLoadNW = value; }
        }
        public Int32 TvTestOpenWait
        {
            get { return tvTestOpenWait; }
            set { tvTestOpenWait = value; }
        }
        public Int32 TvTestChgBonWait
        {
            get { return tvTestChgBonWait; }
            set { tvTestChgBonWait = value; }
        }
        public String ListDefFontColor
        {
            get { return listDefFontColor; }
            set { listDefFontColor = value; }
        }
        public List<String> RecModeFontColorList
        {
            get { return recModeFontColorList; }
            set { recModeFontColorList = value; }
        }
        public byte ResDefColorR
        {
            get { return resDefColorR; }
            set { resDefColorR = value; }
        }
        public byte ResDefColorG
        {
            get { return resDefColorG; }
            set { resDefColorG = value; }
        }
        public byte ResDefColorB
        {
            get { return resDefColorB; }
            set { resDefColorB = value; }
        }
        public byte ResErrColorR
        {
            get { return resErrColorR; }
            set { resErrColorR = value; }
        }
        public byte ResErrColorG
        {
            get { return resErrColorG; }
            set { resErrColorG = value; }
        }
        public byte ResErrColorB
        {
            get { return resErrColorB; }
            set { resErrColorB = value; }
        }
        public byte ResWarColorR
        {
            get { return resWarColorR; }
            set { resWarColorR = value; }
        }
        public byte ResWarColorG
        {
            get { return resWarColorG; }
            set { resWarColorG = value; }
        }
        public byte ResWarColorB
        {
            get { return resWarColorB; }
            set { resWarColorB = value; }
        }
        public byte ResNoColorR
        {
            get { return resNoColorR; }
            set { resNoColorR = value; }
        }
        public byte ResNoColorG
        {
            get { return resNoColorG; }
            set { resNoColorG = value; }
        }
        public byte ResNoColorB
        {
            get { return resNoColorB; }
            set { resNoColorB = value; }
        }
        public byte RecEndDefColorR
        {
            get { return recEndDefColorR; }
            set { recEndDefColorR = value; }
        }
        public byte RecEndDefColorG
        {
            get { return recEndDefColorG; }
            set { recEndDefColorG = value; }
        }
        public byte RecEndDefColorB
        {
            get { return recEndDefColorB; }
            set { recEndDefColorB = value; }
        }
        public byte RecEndErrColorR
        {
            get { return recEndErrColorR; }
            set { recEndErrColorR = value; }
        }
        public byte RecEndErrColorG
        {
            get { return recEndErrColorG; }
            set { recEndErrColorG = value; }
        }
        public byte RecEndErrColorB
        {
            get { return recEndErrColorB; }
            set { recEndErrColorB = value; }
        }
        public byte RecEndWarColorR
        {
            get { return recEndWarColorR; }
            set { recEndWarColorR = value; }
        }
        public byte RecEndWarColorG
        {
            get { return recEndWarColorG; }
            set { recEndWarColorG = value; }
        }
        public byte RecEndWarColorB
        {
            get { return recEndWarColorB; }
            set { recEndWarColorB = value; }
        }
        public String StatResForeColor
        {
            get { return statResForeColor; }
            set { statResForeColor = value; }
        }
        public String StatRecForeColor
        {
            get { return statRecForeColor; }
            set { statRecForeColor = value; }
        }
        public String StatOnAirForeColor
        {
            get { return statOnAirForeColor; }
            set { statOnAirForeColor = value; }
        }
        public byte EpgTipsBackColorR
        {
            get { return epgTipsBackColorR; }
            set { epgTipsBackColorR = value; }
        }
        public byte EpgTipsBackColorG
        {
            get { return epgTipsBackColorG; }
            set { epgTipsBackColorG = value; }
        }
        public byte EpgTipsBackColorB
        {
            get { return epgTipsBackColorB; }
            set { epgTipsBackColorB = value; }
        }
        public byte EpgTipsForeColorR
        {
            get { return epgTipsForeColorR; }
            set { epgTipsForeColorR = value; }
        }
        public byte EpgTipsForeColorG
        {
            get { return epgTipsForeColorG; }
            set { epgTipsForeColorG = value; }
        }
        public byte EpgTipsForeColorB
        {
            get { return epgTipsForeColorB; }
            set { epgTipsForeColorB = value; }
        }
        public bool EpgInfoSingleClick
        {
            get { return epgInfoSingleClick; }
            set { epgInfoSingleClick = value; }
        }
        public byte EpgInfoOpenMode
        {
            get { return epgInfoOpenMode; }
            set { epgInfoOpenMode = value; }
        }
        public UInt32 ExecBat
        {
            get { return execBat; }
            set { execBat = value; }
        }
        public UInt32 SuspendChk
        {
            get { return suspendChk; }
            set { suspendChk = value; }
        }
        public UInt32 SuspendChkTime
        {
            get { return suspendChkTime; }
            set { suspendChkTime = value; }
        }
        public List<ListColumnInfo> ReserveListColumn
        {
            get { return reserveListColumn; }
            set { reserveListColumn = value; }
        }
        public List<ListColumnInfo> RecInfoListColumn
        {
            get { return recInfoListColumn; }
            set { recInfoListColumn = value; }
        }
        public List<ListColumnInfo> AutoAddEpgColumn
        {
            get { return autoAddEpgColumn; }
            set { autoAddEpgColumn = value; }
        }
        public List<ListColumnInfo> AutoAddManualColumn
        {
            get { return autoAddManualColumn; }
            set { autoAddManualColumn = value; }
        }
        public double SearchWndLeft
        {
            get { return searchWndLeft; }
            set { searchWndLeft = value; }
        }
        public double SearchWndTop
        {
            get { return searchWndTop; }
            set { searchWndTop = value; }
        }
        public double SearchWndWidth
        {
            get { return searchWndWidth; }
            set { searchWndWidth = value; }
        }
        public double SearchWndHeight
        {
            get { return searchWndHeight; }
            set { searchWndHeight = value; }
        }
        public short AutoSaveNotifyLog
        {
            get { return autoSaveNotifyLog; }
            set { autoSaveNotifyLog = value; }
        }
        public bool ShowTray
        {
            get { return showTray; }
            set { showTray = value; }
        }
        public bool MinHide
        {
            get { return minHide; }
            set { minHide = value; }
        }
        public bool MouseScrollAuto
        {
            get { return mouseScrollAuto; }
            set { mouseScrollAuto = value; }
        }
        public int NoStyle
        {
            get { return noStyle; }
            set { noStyle = value; }
        }
        public bool CautionManyChange
        {
            get { return cautionManyChange; }
            set { cautionManyChange = value; }
        }
        public int CautionManyNum
        {
            get { return cautionManyNum; }
            set { cautionManyNum = value; }
        }
        public int KeyDeleteDisplayItemNum
        {
            get { return keyDeleteDisplayItemNum; }
            set { keyDeleteDisplayItemNum = value; }
        }        
        public bool DisplayNotifyEpgChange
        {
            get { return displayNotifyEpgChange; }
            set { displayNotifyEpgChange = value; }
        }
        public int DisplayNotifyJumpTime
        {
            get { return displayNotifyJumpTime; }
            set { displayNotifyJumpTime = value; }
        }
        
        public Settings()
        {
            useCustomEpgView = false;
            customEpgTabList = new List<CustomEpgTabInfo>();
            minHeight = 2;
            minimumHeight = 0;
            serviceWidth = 150;
            scrollSize = 240;
            fontName = "メイリオ";
            fontSize = 12;
            fontNameTitle = "メイリオ";
            fontSizeTitle = 12;
            fontBoldTitle = true;
            noToolTip = false;
            playDClick = false;
            dragScroll = 1.5;
            contentColorList = new List<string>();
            contentCustColorList = new List<uint>();
            timeColorList = new List<string>();
            timeCustColorList = new List<uint>();
            reserveRectColorNormal = "Lime";
            reserveRectColorNo = "Black";
            reserveRectColorNoTuner = "Red";
            reserveRectColorWarning = "Yellow";
            titleColor1 = "Black";
            titleColor2 = "Black";
            titleCustColor1 = 0xFFFFFFFF;
            titleCustColor2 = 0xFFFFFFFF;
            serviceColor = "LightSlateGray";
            serviceCustColor = 0xFFFFFFFF;
            reserveRectBackground = false;
            epgTitleIndent = true;
            epgPopup = true;
            epgGradation = true;
            epgGradationHeader = true;
            resColumnHead = "";
            resSortDirection = ListSortDirection.Ascending;
            lastWindowState = System.Windows.WindowState.Normal;
            mainWndLeft = -100;
            mainWndTop = -100;
            mainWndWidth = -100;
            mainWndHeight = -100;
            closeMin = false;
            wakeMin = false;
            viewButtonShowAsTab = false;
            viewButtonList = new List<string>();
            taskMenuList = new List<string>();
            cust1BtnName = "";
            cust1BtnCmd = "";
            cust1BtnCmdOpt = "";
            cust2BtnName = "";
            cust2BtnCmd = "";
            cust2BtnCmdOpt = "";
            andKeyList = new List<string>();
            notKeyList = new List<string>();
            searchKeyRegExp = false;
            searchKeyTitleOnly = false;
            searchKeyAimaiFlag = false;
            searchKeyNotContent = false;
            searchKeyNotDate = false;
            searchKeyFreeCA = 0;
            searchKeyChkRecEnd = 0;
            searchKeyChkRecDay = 6;
            searchKeyChkRecNoService = 0;
            searchKeyChkDurationMin = 0;
            searchKeyChkDurationMax = 0;
            searchKeyContentList = new List<ContentKindInfo>();
            searchKeyDateItemList = new List<DateItem>();
            searchKeyServiceList = new List<Int64>();
            recPresetList = new List<RecPresetItem>();
            recInfoColumnHead = "";
            recInfoSortDirection = ListSortDirection.Ascending;
            tvTestExe = "";
            tvTestCmd = "";
            nwTvMode = false;
            nwTvModeUDP = false;
            nwTvModeTCP = false;
            filePlayExe = "";
            filePlayCmd = "\"$FilePath$\"";
            openFolderWithFileDialog = false;
            iEpgStationList = new List<IEPGStationInfo>();
            menuSet = new MenuSettingData();
            nwServerIP = "";
            nwServerPort = 4510;
            nwWaitPort = 4520;
            nwMacAdd = "";
            wakeReconnectNW = false;
            suspendCloseNW = false;
            ngAutoEpgLoadNW = false;
            tvTestOpenWait = 2000;
            tvTestChgBonWait = 2000;
            listDefFontColor = "#FF042271";
            recModeFontColorList = new List<String>();
            resDefColorR = 0xFF;
            resDefColorG = 0xFF;
            resDefColorB = 0xFF;
            resErrColorR = 0xFF;
            resErrColorG = 0;
            resErrColorB = 0;
            resWarColorR = 0xFF;
            resWarColorG = 0xFF;
            resWarColorB = 0;
            resNoColorR = 0xA9;
            resNoColorG = 0xA9;
            resNoColorB = 0xA9;
            recEndDefColorR = 0xFF;
            recEndDefColorG = 0xFF;
            recEndDefColorB = 0xFF;
            recEndErrColorR = 0xFF;
            recEndErrColorG = 0;
            recEndErrColorB = 0;
            recEndWarColorR = 0xFF;
            recEndWarColorG = 0xFF;
            recEndWarColorB = 0;
            statResForeColor = "RoyalBlue";
            statRecForeColor = "OrangeRed";
            statOnAirForeColor = "LimeGreen";
            epgTipsBackColorR = 0xD3;
            epgTipsBackColorG = 0xD3;
            epgTipsBackColorB = 0xD3;
            epgTipsForeColorR = 0;
            epgTipsForeColorG = 0;
            epgTipsForeColorB = 0;
            epgInfoSingleClick = false;
            epgInfoOpenMode = 0;
            execBat = 0;
            suspendChk = 0;
            suspendChkTime = 15;
            reserveListColumn = new List<ListColumnInfo>();
            recInfoListColumn = new List<ListColumnInfo>();
            autoAddEpgColumn = new List<ListColumnInfo>();
            autoAddManualColumn = new List<ListColumnInfo>();
            searchWndLeft = 0;
            searchWndTop = 0;
            searchWndWidth = 0;
            searchWndHeight = 0;
            autoSaveNotifyLog = 0;
            showTray = true;
            minHide = true;
            mouseScrollAuto = false;
            noStyle = 0;
            cautionManyChange = true;
            cautionManyNum = 10;
            keyDeleteDisplayItemNum = 10;
            displayNotifyEpgChange = false;
            displayNotifyJumpTime = 3;
        }

        [NonSerialized()]
        private static Settings _instance;
        [System.Xml.Serialization.XmlIgnore]
        public static Settings Instance
        {
            get
            {
                if (_instance == null)
                    _instance = new Settings();
                return _instance;
            }
            set { _instance = value; }
        }

        /// <summary>
        /// 設定ファイルロード関数
        /// </summary>
        public static void LoadFromXmlFile(bool nwMode = false)
        {
            _LoadFromXmlFile(GetSettingPath(), nwMode);
        }
        private static void _LoadFromXmlFile(string path, bool nwMode)
        {
            try
            {
                using (var fs = new FileStream(path, FileMode.Open, FileAccess.Read))
                {
                    //読み込んで逆シリアル化する
                    var xs = new System.Xml.Serialization.XmlSerializer(typeof(Settings));
                    Instance = (Settings)(xs.Deserialize(fs));
                }
            }
            catch (Exception ex)
            {
                if (ex.GetBaseException().GetType() != typeof(System.IO.FileNotFoundException))
                {
                    string backPath = path + ".back";
                    if (System.IO.File.Exists(backPath) == true)
                    {
                        if (MessageBox.Show("設定ファイルが異常な可能性があります。\r\nバックアップファイルから読み込みますか？", "", MessageBoxButton.YesNo) == MessageBoxResult.Yes)
                        {
                            _LoadFromXmlFile(backPath, nwMode);
                            return;
                        }
                    }
                    MessageBox.Show(ex.Message + "\r\n" + ex.StackTrace);
                }
            }

            try
            {
                // タイミング合わせにくいので、メニュー系のデータチェックは
                // MenuManager側のワークデータ作成時に実行する。

                if (Instance.recModeFontColorList.Count != 6)
                {
                    //予約アイテムのデフォルトの文字色
                    Instance.recModeFontColorList.Clear();
                    Instance.recModeFontColorList.Add("#FF042271"); //0 全サービス
                    Instance.recModeFontColorList.Add("#FF042271"); //1 指定サービス
                    Instance.recModeFontColorList.Add("#FF042271"); //2 全サービス(デコード処理なし)
                    Instance.recModeFontColorList.Add("#FF042271"); //3 指定サービス(デコード処理なし)
                    Instance.recModeFontColorList.Add("#FF042271"); //4 視聴
                    Instance.recModeFontColorList.Add("#FF042271"); //5 無効
                }
                if (Instance.contentColorList.Count == 0x10)//多分旧バージョンの互換用コード
                {
                    Instance.contentColorList.Add("White");
                }
                else if (Instance.contentColorList.Count != 0x11)
                {
                    //番組表のデフォルトの背景色
                    Instance.contentColorList.Clear();
                    Instance.contentColorList.Add("LightYellow");
                    Instance.contentColorList.Add("Lavender");
                    Instance.contentColorList.Add("LavenderBlush");
                    Instance.contentColorList.Add("MistyRose");
                    Instance.contentColorList.Add("Honeydew");
                    Instance.contentColorList.Add("LightCyan");
                    Instance.contentColorList.Add("PapayaWhip");
                    Instance.contentColorList.Add("Pink");
                    Instance.contentColorList.Add("LightYellow");
                    Instance.contentColorList.Add("PapayaWhip");
                    Instance.contentColorList.Add("AliceBlue");
                    Instance.contentColorList.Add("AliceBlue");
                    Instance.contentColorList.Add("White");
                    Instance.contentColorList.Add("White");
                    Instance.contentColorList.Add("White");
                    Instance.contentColorList.Add("WhiteSmoke");
                    Instance.contentColorList.Add("White");
                }
                if (Instance.ContentCustColorList.Count != 0x11 + 4)
                {
                    Instance.ContentCustColorList.Clear();
                    for (int i = 0; i < 0x11+4; i++)
                    {
                        Instance.ContentCustColorList.Add(0xFFFFFFFF);
                    }
                }
                if (Instance.timeColorList.Count != 4)
                {
                    //番組表の時間軸のデフォルトの背景色
                    Instance.timeColorList.Clear();
                    Instance.timeColorList.Add("MediumPurple");
                    Instance.timeColorList.Add("LightSeaGreen");
                    Instance.timeColorList.Add("LightSalmon");
                    Instance.timeColorList.Add("CornflowerBlue");
                }
                if (Instance.TimeCustColorList.Count != 4)
                {
                    Instance.TimeCustColorList.Clear();
                    for (int i = 0; i < 4; i++)
                    {
                        Instance.TimeCustColorList.Add(0xFFFFFFFF);
                    }
                }
                if (Instance.viewButtonList.Count == 0)
                {
                    if (nwMode == false)
                    {
                        Instance.viewButtonList.Add("設定");
                        Instance.viewButtonList.Add("（空白）");
                        Instance.viewButtonList.Add("検索");
                        Instance.viewButtonList.Add("（空白）");
                        Instance.viewButtonList.Add("スタンバイ");
                        Instance.viewButtonList.Add("休止");
                        Instance.viewButtonList.Add("（空白）");
                        Instance.viewButtonList.Add("EPG取得");
                        Instance.viewButtonList.Add("（空白）");
                        Instance.viewButtonList.Add("EPG再読み込み");
                        Instance.viewButtonList.Add("（空白）");
                        Instance.viewButtonList.Add("終了");
                    }
                    else
                    {
                        Instance.viewButtonList.Add("設定");
                        Instance.viewButtonList.Add("（空白）");
                        Instance.viewButtonList.Add("再接続");
                        Instance.viewButtonList.Add("（空白）");
                        Instance.viewButtonList.Add("検索");
                        Instance.viewButtonList.Add("（空白）");
                        Instance.viewButtonList.Add("EPG取得");
                        Instance.viewButtonList.Add("（空白）");
                        Instance.viewButtonList.Add("EPG再読み込み");
                        Instance.viewButtonList.Add("（空白）");
                        Instance.viewButtonList.Add("終了");
                    }
                }
                if (Instance.taskMenuList.Count == 0)
                {
                    if (nwMode == false)
                    {
                        Instance.taskMenuList.Add("設定");
                        Instance.taskMenuList.Add("（セパレータ）");
                        Instance.taskMenuList.Add("スタンバイ");
                        Instance.taskMenuList.Add("休止");
                        Instance.taskMenuList.Add("（セパレータ）");
                        Instance.taskMenuList.Add("終了");
                    }
                    else
                    {
                        Instance.taskMenuList.Add("設定");
                        Instance.taskMenuList.Add("（セパレータ）");
                        Instance.taskMenuList.Add("再接続");
                        Instance.taskMenuList.Add("（セパレータ）");
                        Instance.taskMenuList.Add("終了");
                    }
                }
                if (Instance.reserveListColumn.Count == 0)
                {
                    Instance.reserveListColumn.Add(new ListColumnInfo("StartTime", double.NaN));
                    Instance.reserveListColumn.Add(new ListColumnInfo("NetworkName", double.NaN));
                    Instance.reserveListColumn.Add(new ListColumnInfo("ServiceName", double.NaN));
                    Instance.reserveListColumn.Add(new ListColumnInfo("EventName", double.NaN));
                    Instance.reserveListColumn.Add(new ListColumnInfo("RecMode", double.NaN));
                    Instance.reserveListColumn.Add(new ListColumnInfo("Priority", double.NaN));
                    Instance.reserveListColumn.Add(new ListColumnInfo("Tuijyu", double.NaN));
                    Instance.reserveListColumn.Add(new ListColumnInfo("Comment", double.NaN));
                    Instance.reserveListColumn.Add(new ListColumnInfo("RecFileName", double.NaN));
                }
                if (Instance.recInfoListColumn.Count == 0)
                {
                    Instance.recInfoListColumn.Add(new ListColumnInfo("IsProtect", double.NaN));
                    Instance.recInfoListColumn.Add(new ListColumnInfo("StartTime", double.NaN));
                    Instance.recInfoListColumn.Add(new ListColumnInfo("NetworkName", double.NaN));
                    Instance.recInfoListColumn.Add(new ListColumnInfo("ServiceName", double.NaN));
                    Instance.recInfoListColumn.Add(new ListColumnInfo("EventName", double.NaN));
                    Instance.recInfoListColumn.Add(new ListColumnInfo("Drops", double.NaN));
                    Instance.recInfoListColumn.Add(new ListColumnInfo("Scrambles", double.NaN));
                    Instance.recInfoListColumn.Add(new ListColumnInfo("Result", double.NaN));
                    Instance.recInfoListColumn.Add(new ListColumnInfo("RecFilePath", double.NaN));
                }
                if (Instance.autoAddEpgColumn.Count == 0)
                {
                    Instance.autoAddEpgColumn.Add(new ListColumnInfo("AndKey", double.NaN));
                    Instance.autoAddEpgColumn.Add(new ListColumnInfo("NotKey", double.NaN));
                    Instance.autoAddEpgColumn.Add(new ListColumnInfo("RegExp", double.NaN));
                    Instance.autoAddEpgColumn.Add(new ListColumnInfo("RecMode", double.NaN));
                    Instance.autoAddEpgColumn.Add(new ListColumnInfo("Priority", double.NaN));
                    Instance.autoAddEpgColumn.Add(new ListColumnInfo("Tuijyu", double.NaN));
                }
                if (Instance.autoAddManualColumn.Count == 0)
                {
                    Instance.autoAddManualColumn.Add(new ListColumnInfo("DayOfWeek", double.NaN));
                    Instance.autoAddManualColumn.Add(new ListColumnInfo("Time", double.NaN));
                    Instance.autoAddManualColumn.Add(new ListColumnInfo("Title", double.NaN));
                    Instance.autoAddManualColumn.Add(new ListColumnInfo("StationName", double.NaN));
                    Instance.autoAddManualColumn.Add(new ListColumnInfo("RecMode", double.NaN));
                    Instance.autoAddManualColumn.Add(new ListColumnInfo("Priority", double.NaN));
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.Message + "\r\n" + ex.StackTrace);
            }
        }
 
        public static void SaveToXmlFile()
        {
            try
            {
                string path = GetSettingPath();

                if (System.IO.File.Exists(path) == true)
                {
                    string backPath = path + ".back";
                    System.IO.File.Copy(path, backPath, true);
                }

                FileStream fs = new FileStream(path,
                    FileMode.Create,
                    FileAccess.Write, FileShare.None);
                System.Xml.Serialization.XmlSerializer xs =
                    new System.Xml.Serialization.XmlSerializer(
                    typeof(Settings));
                //シリアル化して書き込む
                xs.Serialize(fs, Instance);
                fs.Close();
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.Message + "\r\n" + ex.StackTrace);
            }
        }

        private static string GetSettingPath()
        {
            Assembly myAssembly = Assembly.GetEntryAssembly();
            string path = myAssembly.Location + ".xml";

            return path;
        }

        public void SetSettings(string propertyName, object value)
        {
            if (propertyName == null) return;
            var info = typeof(Settings).GetProperty(propertyName);
            if (info != null) info.SetValue(this, value, null);
        }

        public object GetSettings(string propertyName)
        {
            if (propertyName == null) return null;
            var info = typeof(Settings).GetProperty(propertyName);
            return (info == null ? null : info.GetValue(this, null));
        }

        public static void GetDefRecSetting(UInt32 presetID, ref CtrlCmdCLI.Def.RecSettingData defKey)
        {
            String defName = "REC_DEF";
            String defFolderName = "REC_DEF_FOLDER";
            String defFolder1SegName = "REC_DEF_FOLDER_1SEG";

            if (presetID > 0)
            {
                defName += presetID.ToString();
                defFolderName += presetID.ToString();
                defFolder1SegName += presetID.ToString();
            }

            defKey.RecMode = (Byte)IniFileHandler.GetPrivateProfileInt(defName, "RecMode", 1, SettingPath.TimerSrvIniPath);
            defKey.Priority = (Byte)IniFileHandler.GetPrivateProfileInt(defName, "Priority", 2, SettingPath.TimerSrvIniPath);
            defKey.TuijyuuFlag = (Byte)IniFileHandler.GetPrivateProfileInt(defName, "TuijyuuFlag", 1, SettingPath.TimerSrvIniPath);
            defKey.ServiceMode = (Byte)IniFileHandler.GetPrivateProfileInt(defName, "ServiceMode", 0, SettingPath.TimerSrvIniPath);
            defKey.PittariFlag = (Byte)IniFileHandler.GetPrivateProfileInt(defName, "PittariFlag", 0, SettingPath.TimerSrvIniPath);

            defKey.BatFilePath = IniFileHandler.GetPrivateProfileString(defName, "BatFilePath", "", SettingPath.TimerSrvIniPath);

            defKey.RecFolderList.Clear();
            int count = IniFileHandler.GetPrivateProfileInt(defFolderName, "Count", 0, SettingPath.TimerSrvIniPath);
            for (int i = 0; i < count; i++)
            {
                CtrlCmdCLI.Def.RecFileSetInfo folderInfo = new CtrlCmdCLI.Def.RecFileSetInfo();
                folderInfo.RecFolder = IniFileHandler.GetPrivateProfileString(defFolderName, i.ToString(), "", SettingPath.TimerSrvIniPath);
                folderInfo.WritePlugIn = IniFileHandler.GetPrivateProfileString(defFolderName, "WritePlugIn" + i.ToString(), "Write_Default.dll", SettingPath.TimerSrvIniPath);
                folderInfo.RecNamePlugIn = IniFileHandler.GetPrivateProfileString(defFolderName, "RecNamePlugIn" + i.ToString(), "", SettingPath.TimerSrvIniPath);

                defKey.RecFolderList.Add(folderInfo);
            }

            defKey.PartialRecFolder.Clear();
            count = IniFileHandler.GetPrivateProfileInt(defFolder1SegName, "Count", 0, SettingPath.TimerSrvIniPath);
            for (int i = 0; i < count; i++)
            {
                CtrlCmdCLI.Def.RecFileSetInfo folderInfo = new CtrlCmdCLI.Def.RecFileSetInfo();
                folderInfo.RecFolder = IniFileHandler.GetPrivateProfileString(defFolder1SegName, i.ToString(), "", SettingPath.TimerSrvIniPath);
                folderInfo.WritePlugIn = IniFileHandler.GetPrivateProfileString(defFolder1SegName, "WritePlugIn" + i.ToString(), "Write_Default.dll", SettingPath.TimerSrvIniPath);
                folderInfo.RecNamePlugIn = IniFileHandler.GetPrivateProfileString(defFolder1SegName, "RecNamePlugIn" + i.ToString(), "", SettingPath.TimerSrvIniPath);

                defKey.PartialRecFolder.Add(folderInfo);
            }

            defKey.SuspendMode = (Byte)IniFileHandler.GetPrivateProfileInt(defName, "SuspendMode", 0, SettingPath.TimerSrvIniPath);
            defKey.RebootFlag = (Byte)IniFileHandler.GetPrivateProfileInt(defName, "RebootFlag", 0, SettingPath.TimerSrvIniPath);
            defKey.UseMargineFlag = (Byte)IniFileHandler.GetPrivateProfileInt(defName, "UseMargineFlag", 0, SettingPath.TimerSrvIniPath);
            defKey.StartMargine = IniFileHandler.GetPrivateProfileInt(defName, "StartMargine", 0, SettingPath.TimerSrvIniPath);
            defKey.EndMargine = IniFileHandler.GetPrivateProfileInt(defName, "EndMargine", 0, SettingPath.TimerSrvIniPath);
            defKey.ContinueRecFlag = (Byte)IniFileHandler.GetPrivateProfileInt(defName, "ContinueRec", 0, SettingPath.TimerSrvIniPath);
            defKey.PartialRecFlag = (Byte)IniFileHandler.GetPrivateProfileInt(defName, "PartialRec", 0, SettingPath.TimerSrvIniPath);
            defKey.TunerID = (UInt32)IniFileHandler.GetPrivateProfileInt(defName, "TunerID", 0, SettingPath.TimerSrvIniPath);

        }

        //プリセットの更新
        public static void UpdateDefRecSetting()
        {
            Settings.Instance.RecPresetList.Clear();
            string pIDs = "0," + IniFileHandler.GetPrivateProfileString("SET", "PresetID", "", SettingPath.TimerSrvIniPath);
            foreach (string pID in pIDs.Split(','))
            {
                uint id;
                if (uint.TryParse(pID, out id) == false) continue;
                string name = IniFileHandler.GetPrivateProfileString("REC_DEF" + (id == 0 ? "" : id.ToString()), "SetName", "", SettingPath.TimerSrvIniPath);
                Settings.Instance.RecPresetList.Add(new RecPresetItem(name, id));
            }
        }

        public static void GetDefSearchSetting(ref CtrlCmdCLI.Def.EpgSearchKeyInfo defKey)
        {
            if (Settings.Instance.SearchKeyRegExp == true)
            {
                defKey.regExpFlag = 1;
            }
            if (Settings.Instance.SearchKeyAimaiFlag == true)
            {
                defKey.aimaiFlag = 1;
            }
            if (Settings.Instance.SearchKeyTitleOnly == true)
            {
                defKey.titleOnlyFlag = 1;
            }
            if (Settings.Instance.SearchKeyNotContent == true)
            {
                defKey.notContetFlag = 1;
            }
            if (Settings.Instance.SearchKeyNotDate == true)
            {
                defKey.notDateFlag = 1;
            }
            foreach (ContentKindInfo info in Settings.Instance.SearchKeyContentList)
            {
                CtrlCmdCLI.Def.EpgContentData item = new CtrlCmdCLI.Def.EpgContentData();
                item.content_nibble_level_1 = info.Nibble1;
                item.content_nibble_level_2 = info.Nibble2;
                defKey.contentList.Add(item);
            }
            foreach (DateItem info in Settings.Instance.SearchKeyDateItemList)
            {
                defKey.dateList.Add(info.DateInfo);
            }
            foreach (Int64 info in Settings.Instance.SearchKeyServiceList)
            {
                defKey.serviceList.Add(info);
            }
            defKey.freeCAFlag = Settings.Instance.SearchKeyFreeCA;
            defKey.chkRecEnd = Settings.Instance.SearchKeyChkRecEnd;
            defKey.chkRecDay = Settings.Instance.SearchKeyChkRecDay;
            defKey.chkRecNoService = Settings.Instance.SearchKeyChkRecNoService;
            defKey.chkDurationMin = Settings.Instance.SearchKeyChkDurationMin;
            defKey.chkDurationMax = Settings.Instance.SearchKeyChkDurationMax;
        }

        public static List<string> GetDefRecFolders()
        {
            var folders = new List<string>();
            int num = IniFileHandler.GetPrivateProfileInt("SET", "RecFolderNum", 0, SettingPath.CommonIniPath);
            if (num == 0)
            {
                folders.Add(IniFileHandler.GetPrivateProfileString("SET", "DataSavePath", "Setting", SettingPath.CommonIniPath));
            }
            else
            {
                for (uint i = 0; i < num; i++)
                {
                    string key = "RecFolderPath" + i.ToString();
                    string folder = IniFileHandler.GetPrivateProfileString("SET", key, "", SettingPath.CommonIniPath);
                    if (folder.Length > 0)
                    {
                        folders.Add(folder);
                    }
                }
            }
            return folders;
        }
    }
}
