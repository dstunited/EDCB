﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

using CtrlCmdCLI;
using CtrlCmdCLI.Def;

namespace EpgTimer
{
    static class CtrlCmdCLIEx
    {
        //シリアライズとかもろもろは使えないので地道にコピーする。
        //シャローコピー部分はGetType()などを使う方法もあるが、やはり地道にコピーすることにする。

        //CopyObj.csのジェネリックを使って定義している。
        public static List<EpgSearchKeyInfo> Clone(this List<EpgSearchKeyInfo> src) { return CopyObj.Clone(src, CopyData); }
        public static EpgSearchKeyInfo Clone(this EpgSearchKeyInfo src) { return CopyObj.Clone(src, CopyData); }
        public static void CopyTo(this EpgSearchKeyInfo src, EpgSearchKeyInfo dest) { CopyObj.CopyTo(src, dest, CopyData); }
        private static void CopyData(EpgSearchKeyInfo src, EpgSearchKeyInfo dest)
        {
            dest.aimaiFlag = src.aimaiFlag;
            dest.andKey = src.andKey;
            dest.audioList = src.audioList.ToList();
            dest.contentList = src.contentList.Clone(); //EpgContentData
            dest.dateList = src.dateList.Clone();       //EpgSearchDateInfo
            dest.freeCAFlag = src.freeCAFlag;
            dest.notContetFlag = src.notContetFlag;
            dest.notDateFlag = src.notDateFlag;
            dest.notKey = src.notKey;
            dest.regExpFlag = src.regExpFlag;
            dest.serviceList = src.serviceList.ToList();
            dest.titleOnlyFlag = src.titleOnlyFlag;
            dest.videoList = src.videoList.ToList();
            dest.chkRecEnd = src.chkRecEnd;
            dest.chkRecDay = src.chkRecDay;
            dest.chkRecNoService = src.chkRecNoService;
            dest.chkDurationMin = src.chkDurationMin;
            dest.chkDurationMax = src.chkDurationMax;
        }

        public static List<EpgContentData> Clone(this List<EpgContentData> src) { return CopyObj.Clone(src, CopyData); }
        public static EpgContentData Clone(this EpgContentData src) { return CopyObj.Clone(src, CopyData); }
        public static void CopyTo(this EpgContentData src, EpgContentData dest) { CopyObj.CopyTo(src, dest, CopyData); }
        private static void CopyData(EpgContentData src, EpgContentData dest)
        {
            dest.content_nibble_level_1 = src.content_nibble_level_1;
            dest.content_nibble_level_2 = src.content_nibble_level_2;
            dest.user_nibble_1 = src.user_nibble_1;
            dest.user_nibble_2 = src.user_nibble_2;
        }

        public static List<EpgSearchDateInfo> Clone(this List<EpgSearchDateInfo> src) { return CopyObj.Clone(src, CopyData); }
        public static EpgSearchDateInfo Clone(this EpgSearchDateInfo src) { return CopyObj.Clone(src, CopyData); }
        public static void CopyTo(this EpgSearchDateInfo src, EpgSearchDateInfo dest) { CopyObj.CopyTo(src, dest, CopyData); }
        private static void CopyData(EpgSearchDateInfo src, EpgSearchDateInfo dest)
        {
            dest.endDayOfWeek = src.endDayOfWeek;
            dest.endHour = src.endHour;
            dest.endMin = src.endMin;
            dest.startDayOfWeek = src.startDayOfWeek;
            dest.startHour = src.startHour;
            dest.startMin = src.startMin;
        }

        public static List<RecFileInfo> Clone(this List<RecFileInfo> src) { return CopyObj.Clone(src, CopyData); }
        public static RecFileInfo Clone(this RecFileInfo src) { return CopyObj.Clone(src, CopyData); }
        public static void CopyTo(this RecFileInfo src, RecFileInfo dest) { CopyObj.CopyTo(src, dest, CopyData); }
        private static void CopyData(RecFileInfo src, RecFileInfo dest)
        {
            dest.Comment = src.Comment;
            dest.Drops = src.Drops;
            dest.DurationSecond = src.DurationSecond;
            dest.ErrInfo = src.ErrInfo;
            dest.EventID = src.EventID;
            dest.ID = src.ID;
            dest.OriginalNetworkID = src.OriginalNetworkID;
            dest.ProgramInfo = src.ProgramInfo;
            dest.ProtectFlag = src.ProtectFlag;
            dest.RecFilePath = src.RecFilePath;
            dest.RecStatus = src.RecStatus;
            dest.Scrambles = src.Scrambles;
            dest.ServiceID = src.ServiceID;
            dest.ServiceName = src.ServiceName;
            dest.StartTime = src.StartTime;
            dest.StartTimeEpg = src.StartTimeEpg;
            dest.Title = src.Title;
            dest.TransportStreamID = src.TransportStreamID;
        }

        public static List<ReserveData> Clone(this List<ReserveData> src) { return CopyObj.Clone(src, CopyData); }
        public static ReserveData Clone(this ReserveData src) { return CopyObj.Clone(src, CopyData); }
        public static void CopyTo(this ReserveData src, ReserveData dest) { CopyObj.CopyTo(src, dest, CopyData); }
        private static void CopyData(ReserveData src, ReserveData dest)
        {
            dest.Comment = src.Comment;
            dest.DurationSecond = src.DurationSecond;
            dest.EventID = src.EventID;
            dest.OriginalNetworkID = src.OriginalNetworkID;
            dest.OverlapMode = src.OverlapMode;
            dest.param1 = src.param1;
            dest.RecFileNameList = src.RecFileNameList.ToList();
            dest.RecFilePath = src.RecFilePath;
            dest.RecSetting = src.RecSetting.Clone();
            dest.RecWaitFlag = src.RecWaitFlag;
            dest.ReserveID = src.ReserveID;
            dest.ReserveStatus = src.ReserveStatus;
            dest.ServiceID = src.ServiceID;
            dest.StartTime = src.StartTime;
            dest.StartTimeEpg = src.StartTimeEpg;
            dest.StationName = src.StationName;
            dest.Title = src.Title;
            dest.TransportStreamID = src.TransportStreamID;
        }

        public static List<RecSettingData> Clone(this List<RecSettingData> src) { return CopyObj.Clone(src, CopyData); }
        public static RecSettingData Clone(this RecSettingData src) { return CopyObj.Clone(src, CopyData); }
        public static void CopyTo(this RecSettingData src, RecSettingData dest) { CopyObj.CopyTo(src, dest, CopyData); }
        private static void CopyData(RecSettingData src, RecSettingData dest)
        {
            dest.BatFilePath = src.BatFilePath;
            dest.ContinueRecFlag = src.ContinueRecFlag;
            dest.EndMargine = src.EndMargine;
            dest.PartialRecFlag = src.PartialRecFlag;
            dest.PartialRecFolder = src.PartialRecFolder.Clone();
            dest.PittariFlag = src.PittariFlag;
            dest.Priority = src.Priority;
            dest.RebootFlag = src.RebootFlag;
            dest.RecFolderList = src.RecFolderList.Clone();
            dest.RecMode = src.RecMode;
            dest.ServiceMode = src.ServiceMode;
            dest.StartMargine = src.StartMargine;
            dest.SuspendMode = src.SuspendMode;
            dest.TuijyuuFlag = src.TuijyuuFlag;
            dest.TunerID = src.TunerID;
            dest.UseMargineFlag = src.UseMargineFlag;
        }

        public static List<RecFileSetInfo> Clone(this List<RecFileSetInfo> src) { return CopyObj.Clone(src, CopyData); }
        public static RecFileSetInfo Clone(this RecFileSetInfo src) { return CopyObj.Clone(src, CopyData); }
        public static void CopyTo(this RecFileSetInfo src, RecFileSetInfo dest) { CopyObj.CopyTo(src, dest, CopyData); }
        private static void CopyData(RecFileSetInfo src, RecFileSetInfo dest)
        {
            dest.RecFileName = src.RecFileName;
            dest.RecFolder = src.RecFolder;
            dest.RecNamePlugIn = src.RecNamePlugIn;
            dest.WritePlugIn = src.WritePlugIn;
        }

        public static List<EpgAutoAddData> Clone(this List<EpgAutoAddData> src) { return CopyObj.Clone(src, CopyData); }
        public static EpgAutoAddData Clone(this EpgAutoAddData src) { return CopyObj.Clone(src, CopyData); }
        public static void CopyTo(this EpgAutoAddData src, EpgAutoAddData dest) { CopyObj.CopyTo(src, dest, CopyData); }
        private static void CopyData(EpgAutoAddData src, EpgAutoAddData dest)
        {
            dest.addCount = src.addCount;
            dest.dataID = src.dataID;
            dest.recSetting = src.recSetting.Clone();
            dest.searchInfo = src.searchInfo.Clone();
        }

        public static List<ManualAutoAddData> Clone(this List<ManualAutoAddData> src) { return CopyObj.Clone(src, CopyData); }
        public static ManualAutoAddData Clone(this ManualAutoAddData src) { return CopyObj.Clone(src, CopyData); }
        public static void CopyTo(this ManualAutoAddData src, ManualAutoAddData dest) { CopyObj.CopyTo(src, dest, CopyData); }
        private static void CopyData(ManualAutoAddData src, ManualAutoAddData dest)
        {
            dest.dataID = src.dataID;
            dest.dayOfWeekFlag = src.dayOfWeekFlag;
            dest.durationSecond = src.durationSecond;
            dest.originalNetworkID = src.originalNetworkID;
            dest.recSetting = src.recSetting.Clone();
            dest.serviceID = src.serviceID;
            dest.startTime = src.startTime;
            dest.stationName = src.stationName;
            dest.title = src.title;
            dest.transportStreamID = src.transportStreamID;
}

        public static List<RecSettingData> RecSettingList(this List<ReserveData> list)
        {
            return list.Where(item => item != null).Select(item => item.RecSetting).ToList();
        }
        public static List<RecSettingData> RecSettingList(this List<EpgAutoAddData> list)
        {
            return list.Where(item => item != null).Select(item => item.recSetting).ToList();
        }
        public static List<RecSettingData> RecSettingList(this List<ManualAutoAddData> list)
        {
            return list.Where(item => item != null).Select(item => item.recSetting).ToList();
        }

        public static List<EpgSearchKeyInfo> RecSearchKeyList(this List<EpgAutoAddData> list)
        {
            return list.Where(item => item != null).Select(item => item.searchInfo).ToList();
        }

        public static UInt64 Create64Key(this EpgServiceInfo obj)
        {
            return CommonManager.Create64Key(obj.ONID, obj.TSID, obj.SID);
        }

        public static UInt64 Create64Key(this RecFileInfo obj)
        {
            return CommonManager.Create64Key(obj.OriginalNetworkID, obj.TransportStreamID, obj.ServiceID);
        }
        public static UInt64 Create64PgKey(this RecFileInfo obj)
        {
            return CommonManager.Create64PgKey(obj.OriginalNetworkID, obj.TransportStreamID, obj.ServiceID, obj.EventID);
        }

        public static UInt64 Create64Key(this ManualAutoAddData obj)
        {
            return CommonManager.Create64Key(obj.originalNetworkID, obj.transportStreamID, obj.serviceID);
        }

        public static UInt64 Create64Key(this EpgEventInfo obj)
        {
            return CommonManager.Create64Key(obj.original_network_id, obj.transport_stream_id, obj.service_id);
        }
        public static UInt64 Create64PgKey(this EpgEventInfo obj)
        {
            return CommonManager.Create64PgKey(obj.original_network_id, obj.transport_stream_id, obj.service_id, obj.event_id);
        }
        public static string Title(this EpgEventInfo info)
        {
            return (info.ShortInfo == null ? "" : info.ShortInfo.event_name);
        }
        
        public static UInt64 Create64Key(this EpgEventData obj)
        {
            return CommonManager.Create64Key(obj.original_network_id, obj.transport_stream_id, obj.service_id);
        }
        public static UInt64 Create64PgKey(this EpgEventData obj)
        {
            return CommonManager.Create64PgKey(obj.original_network_id, obj.transport_stream_id, obj.service_id, obj.event_id);
        }

        public static UInt64 Create64Key(this ReserveData obj)
        {
            return CommonManager.Create64Key(obj.OriginalNetworkID, obj.TransportStreamID, obj.ServiceID);
        }
        public static UInt64 Create64PgKey(this ReserveData obj)
        {
            return CommonManager.Create64PgKey(obj.OriginalNetworkID, obj.TransportStreamID, obj.ServiceID, obj.EventID);
        }

        public static bool IsOnRec(this ReserveData reserveInfo)
        {
            if (reserveInfo == null) return false;
            //
            DateTime startTime = reserveInfo.StartTime;
            int duration = (int)reserveInfo.DurationSecond;
            int StartMargin = CommonManager.Instance.MUtil.GetMargin(reserveInfo.RecSetting, true);
            int EndMargin = CommonManager.Instance.MUtil.GetMargin(reserveInfo.RecSetting, false);

            startTime = reserveInfo.StartTime.AddSeconds(StartMargin * -1);
            duration += StartMargin;
            duration += EndMargin;

            return isOnTime(startTime, duration);
        }

        public static bool IsOnAir(this ReserveData reserveInfo)
        {
            if (reserveInfo == null) return false;
            //
            return isOnTime(reserveInfo.StartTime, (int)reserveInfo.DurationSecond);
        }
        public static bool IsOnAir(this EpgEventInfo eventInfo)
        {
            if (eventInfo == null) return false;
            //
            return isOnTime(eventInfo.start_time, (int)eventInfo.durationSec);
        }

        private static bool isOnTime(DateTime startTime, int duration)
        {
            if (startTime > System.DateTime.Now) return false;
            //
            return (startTime + TimeSpan.FromSeconds(duration) >= System.DateTime.Now);
        }

        public static List<ReserveData> GetReserveList(this ManualAutoAddData mdata)
        {
            return CommonManager.Instance.DB.ReserveList.Values.Where(info =>
            {
                //イベントID無し、サービス、開始時刻、長さ一致の予約を拾う。EpgTimerSrv側と同じ内容の判定。
                return info != null && info.EventID == 0xFFFF
                    && mdata.Create64Key() == info.Create64Key()
                    && mdata.startTime == info.StartTime.Hour * 3600 + info.StartTime.Minute * 60 + info.StartTime.Second
                    && mdata.durationSecond == info.DurationSecond
                    && (mdata.dayOfWeekFlag & (byte)(0x01 << (int)info.StartTime.DayOfWeek)) != 0
                    ;
            }).ToList();
        }

        public static List<RecFileInfo> NoProtectedInfoList(this ICollection<RecFileInfo> itemlist)
        {
            return itemlist.Where(item => item == null ? false : item.ProtectFlag == false).ToList();
        }
        //public static bool HasProtected(this List<RecInfoItem> list)
        //{
        //    return list.Any(info => info == null ? false : info.RecInfo.ProtectFlag == true);
        //}
        public static bool HasNoProtected(this List<RecFileInfo> list)
        {
            return list.Any(info => info == null ? false : info.ProtectFlag == false);
        }
    }
}
