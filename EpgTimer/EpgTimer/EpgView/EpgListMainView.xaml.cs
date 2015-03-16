﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;
using System.ComponentModel;
using System.Collections.ObjectModel;
using System.Collections;
using System.Text.RegularExpressions;

using CtrlCmdCLI;
using CtrlCmdCLI.Def;


namespace EpgTimer
{
    /// <summary>
    /// EpgListMainView.xaml の相互作用ロジック
    /// </summary>
    public partial class EpgListMainView : UserControl
    {
        public event ViewSettingClickHandler ViewSettingClick = null;

        private CustomEpgTabInfo setViewInfo = null;

        private List<UInt64> viewCustServiceList = null;
        private Dictionary<UInt16, UInt16> viewCustContentKindList = new Dictionary<UInt16, UInt16>();
        private List<SearchItem> programList = new List<SearchItem>();
        private List<ServiceItem> serviceList = new List<ServiceItem>();

        string _lastHeaderClicked = null;
        ListSortDirection _lastDirection = ListSortDirection.Ascending;
        string _lastHeaderClicked2 = null;
        ListSortDirection _lastDirection2 = ListSortDirection.Ascending;
        
        private CtrlCmdUtil cmd = CommonManager.Instance.CtrlCmd;


        private Dictionary<UInt64, EpgServiceEventInfo> serviceEventList = new Dictionary<UInt64, EpgServiceEventInfo>();

        private bool updateEpgData = true;
        private bool updateReserveData = true;

        private Dictionary<UInt64, UInt64> lastChkSID = null;

        public EpgListMainView()
        {
            InitializeComponent();
        }

        public bool ClearInfo()
        {
            if (lastChkSID != null && listBox_service.ItemsSource != null)
            {
                lastChkSID.Clear();
                foreach (ServiceItem info in serviceList)
                {
                    if (info.IsSelected == true)
                    {
                        lastChkSID.Add(info.ID, info.ID);
                    }
                }
            }
            listBox_service.ItemsSource = null;
            serviceList.Clear();
            listView_event.ItemsSource = null;
            programList.Clear();
            serviceEventList.Clear();
            richTextBox_eventInfo.Document.Blocks.Clear();

            return true;
        }

        public void SetViewMode(CustomEpgTabInfo setInfo)
        {
            ClearInfo();
            setViewInfo = setInfo;
            this.viewCustServiceList = setInfo.ViewServiceList;
            this.viewCustContentKindList.Clear();
            if (setInfo.ViewContentKindList != null)
            {
                foreach (UInt16 val in setInfo.ViewContentKindList)
                {
                    this.viewCustContentKindList.Add(val, val);
                }
            }

            if (ReloadEpgData() == true)
            {
                updateEpgData = false;
                if (ReloadReserveData() == true)
                {
                    updateReserveData = false;
                }
            }
        }

        private void UserControl_Loaded(object sender, RoutedEventArgs e)
        {
            if (this.IsVisible == true)
            {
                if (updateEpgData == true)
                {
                    if (ReloadEpgData() == true)
                    {
                        updateEpgData = false;
                        if (ReloadReserveData() == true)
                        {
                            updateReserveData = false;
                        }
                    }
                }
                if (updateReserveData == true)
                {
                    if (ReloadReserveData() == true)
                    {
                        updateReserveData = false;
                    }
                }
            }
        }

        /// <summary>
        /// EPGデータ更新通知
        /// </summary>
        public void UpdateEpgData()
        {
            updateEpgData = true;
            if (this.IsVisible == true || CommonManager.Instance.NWMode == false)
            {
                ClearInfo();
                if (ReloadEpgData() == true)
                {
                    updateEpgData = false;
                    if (ReloadReserveData() == true)
                    {
                        updateReserveData = false;
                    }
                }
            }
        }

        /// <summary>
        /// 予約情報更新通知
        /// </summary>
        public void UpdateReserveData()
        {
            updateReserveData = true;
            if (this.IsVisible == true)
            {
                if (ReloadReserveData() == true)
                {
                    updateReserveData = false;
                }
            }
        }

        private bool ReloadEpgData()
        {
            try
            {
                if (setViewInfo != null)
                {
                    if (lastChkSID != null && listBox_service.ItemsSource != null)
                    {
                        lastChkSID.Clear();
                        foreach (ServiceItem info in serviceList)
                        {
                            if (info.IsSelected == true)
                            {
                                lastChkSID.Add(info.ID, info.ID);
                            }
                        }
                    }
                    listBox_service.ItemsSource = null;
                    serviceList.Clear();
                    listView_event.ItemsSource = null;
                    programList.Clear();
                    serviceEventList.Clear();

                    updateEpgData = false;
                    if (setViewInfo.SearchMode == true)
                    {
                        ReloadProgramViewItemForSearch();
                    }
                    else
                    {
                        if (CommonManager.Instance.NWMode == true)
                        {
                            if (CommonManager.Instance.NW.IsConnected == false)
                            {
                                return false;
                            }
                        }
                        ErrCode err = CommonManager.Instance.DB.ReloadEpgData();
                        if (err == ErrCode.CMD_ERR_CONNECT)
                        {
                            if (this.IsVisible == true)
                            {
                                this.Dispatcher.BeginInvoke(new Action(() =>
                                {
                                    MessageBox.Show("サーバー または EpgTimerSrv に接続できませんでした。");
                                }), null);
                            }
                            return false;
                        }
                        if (err == ErrCode.CMD_ERR_BUSY)
                        {
                            /*if (this.IsVisible == true)
                            {
                                this.Dispatcher.BeginInvoke(new Action(() =>
                                {
                                    MessageBox.Show("EPGデータの読み込みを行える状態ではありません。\r\n（EPGデータ読み込み中。など）");
                                }), null);
                            }*/
                            return false;
                        }
                        if (err == ErrCode.CMD_ERR_TIMEOUT)
                        {
                            if (this.IsVisible == true)
                            {
                                this.Dispatcher.BeginInvoke(new Action(() =>
                                {
                                    MessageBox.Show("EpgTimerSrvとの接続にタイムアウトしました。");
                                }), null);
                            }
                            return false;
                        }
                        if (err != ErrCode.CMD_SUCCESS)
                        {
                            if (this.IsVisible == true)
                            {
                                this.Dispatcher.BeginInvoke(new Action(() =>
                                {
                                    MessageBox.Show("EPGデータの取得でエラーが発生しました。EPGデータが読み込まれていない可能性があります。");
                                }), null);
                            }
                            return false; 
                        }

                        ReloadProgramViewItem();
                    }
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.Message + "\r\n" + ex.StackTrace);
            }
            return true;
        }

        private bool ReloadReserveData()
        {
            try
            {
                if (CommonManager.Instance.NWMode == true)
                {
                    if (CommonManager.Instance.NW.IsConnected == false)
                    {
                        return false;
                    }
                }
                ErrCode err = CommonManager.Instance.DB.ReloadReserveInfo();
                if (err == ErrCode.CMD_ERR_CONNECT)
                {
                    MessageBox.Show("サーバー または EpgTimerSrv に接続できませんでした。");
                    return false;
                }
                if (err == ErrCode.CMD_ERR_TIMEOUT)
                {
                    MessageBox.Show("EpgTimerSrvとの接続にタイムアウトしました。");
                    return false;
                }
                if (err != ErrCode.CMD_SUCCESS)
                {
                    MessageBox.Show("予約情報の取得でエラーが発生しました。");
                    return false;
                }

                if (setViewInfo.SearchMode == true)
                {
                    ReloadProgramViewItemForSearch();
                }
                else
                {
                    ReloadProgramViewItem();
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.Message + "\r\n" + ex.StackTrace);
            }
            return true;
        }

        private void ReloadProgramViewItem()
        {
            try
            {
                if (lastChkSID != null && listBox_service.ItemsSource != null)
                {
                    lastChkSID.Clear();
                    foreach (ServiceItem info in serviceList)
                    {
                        if (info.IsSelected == true)
                        {
                            lastChkSID.Add(info.ID, info.ID);
                        }
                    }
                } 
                listBox_service.ItemsSource = null;
                serviceList.Clear();

                foreach (UInt64 id in viewCustServiceList)
                {
                    if (CommonManager.Instance.DB.ServiceEventList.ContainsKey(id) == true)
                    {
                        ServiceItem item = new ServiceItem();
                        item.ServiceInfo = CommonManager.Instance.DB.ServiceEventList[id].serviceInfo;
                        item.IsSelected = true;
                        if (lastChkSID != null)
                        {
                            if (lastChkSID.ContainsKey(id) == false)
                            {
                                item.IsSelected = false;
                            }
                        }
                        serviceList.Add(item);
                    }
                }
                if (lastChkSID == null)
                {
                    lastChkSID = new Dictionary<ulong, ulong>();
                }

                listBox_service.ItemsSource = serviceList;

                UpdateEventList();
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.Message + "\r\n" + ex.StackTrace);
            }
        }

        private void ReloadProgramViewItemForSearch()
        {
            try
            {
                if (lastChkSID != null && listBox_service.ItemsSource != null)
                {
                    lastChkSID.Clear();
                    foreach (ServiceItem info in serviceList)
                    {
                        if (info.IsSelected == true)
                        {
                            lastChkSID.Add(info.ID, info.ID);
                        }
                    }
                }
                listBox_service.ItemsSource = null;
                serviceList.Clear();

                //番組情報の検索
                List<EpgSearchKeyInfo> keyList = new List<EpgSearchKeyInfo>();
                keyList.Add(setViewInfo.SearchKey);
                List<EpgEventInfo> list = new List<EpgEventInfo>();

                cmd.SendSearchPg(keyList, ref list);

                //サービス毎のリストに変換
                serviceEventList.Clear();
                foreach (EpgEventInfo eventInfo in list)
                {
                    UInt64 id = CommonManager.Create64Key(eventInfo.original_network_id, eventInfo.transport_stream_id, eventInfo.service_id);
                    EpgServiceEventInfo serviceInfo = null;
                    if (serviceEventList.ContainsKey(id) == false)
                    {
                        if (ChSet5.Instance.ChList.ContainsKey(id) == false)
                        {
                            //サービス情報ないので無効
                            continue;
                        }
                        serviceInfo = new EpgServiceEventInfo();
                        serviceInfo.serviceInfo = CommonManager.ConvertChSet5To(ChSet5.Instance.ChList[id]);

                        serviceEventList.Add(id, serviceInfo);
                    }
                    else
                    {
                        serviceInfo = serviceEventList[id];
                    }
                    serviceInfo.eventList.Add(eventInfo);
                }

                foreach (UInt64 id in viewCustServiceList)
                {
                    if (serviceEventList.ContainsKey(id) == true)
                    {
                        ServiceItem item = new ServiceItem();
                        item.ServiceInfo = serviceEventList[id].serviceInfo;
                        item.IsSelected = true;
                        if (lastChkSID != null)
                        {
                            if (lastChkSID.ContainsKey(id) == false)
                            {
                                item.IsSelected = false;
                            }
                        }
                        serviceList.Add(item);
                    }
                }
                if (lastChkSID == null)
                {
                    lastChkSID = new Dictionary<ulong, ulong>();
                }

                listBox_service.ItemsSource = serviceList;

                UpdateEventList();
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.Message + "\r\n" + ex.StackTrace);
            }
        }

        private void UpdateEventList()
        {
            try
            {
                //更新前の選択情報の保存。
                //なお、EPG更新の場合はReloadEpgData()でも追加で保存・復元コードを実施する必要があるが、
                //大きく番組表が変化するEPG更新前後で選択情報を保存する意味もないのでほっておくことにする。
                EpgEventInfo oldItem = null;
                //List<EpgEventInfo> oldItems = new List<EpgEventInfo>();
                StoreListViewSelected(ref oldItem);//, ref oldItems);

                ICollectionView dataView = CollectionViewSource.GetDefaultView(listView_event.DataContext);
                if (dataView != null)
                {
                    dataView.SortDescriptions.Clear();
                    dataView.Refresh();
                }
                listView_event.DataContext = null;

                //listView_event.ItemsSource = null;
                programList.Clear();

                Dictionary<UInt64, EpgServiceEventInfo> eventList = null;
                if (setViewInfo.SearchMode == true)
                {
                    eventList = serviceEventList;
                }
                else
                {
                    eventList = CommonManager.Instance.DB.ServiceEventList;
                }

                DateTime now = DateTime.Now;
                foreach (ServiceItem info in serviceList)
                {
                    if (info.IsSelected == true)
                    {
                        if (eventList.ContainsKey(info.ID) == true)
                        {
                            foreach (EpgEventInfo eventInfo in eventList[info.ID].eventList)
                            {
                                if (eventInfo.StartTimeFlag == 0)
                                {
                                    //開始未定は除外
                                    continue;
                                }
                                if (setViewInfo.FilterEnded)
                                {
                                    if (eventInfo.start_time.AddSeconds(eventInfo.durationSec) < now)
                                        continue;
                                }
                                //ジャンル絞り込み
                                if (this.viewCustContentKindList.Count > 0)
                                {
                                    bool find = false;
                                    if (eventInfo.ContentInfo != null)
                                    {
                                        if (eventInfo.ContentInfo.nibbleList.Count > 0)
                                        {
                                            foreach (EpgContentData contentInfo in eventInfo.ContentInfo.nibbleList)
                                            {
                                                UInt16 ID1 = (UInt16)(((UInt16)contentInfo.content_nibble_level_1) << 8 | 0xFF);
                                                UInt16 ID2 = (UInt16)(((UInt16)contentInfo.content_nibble_level_1) << 8 | contentInfo.content_nibble_level_2);
                                                if (ID2 == 0x0e01)
                                                {
                                                    ID1 = (UInt16)(((UInt16)contentInfo.user_nibble_1) << 8 | 0x70FF);
                                                    ID2 = (UInt16)(((UInt16)contentInfo.user_nibble_1) << 8 | 0x7000 | contentInfo.user_nibble_2);
                                                }
                                                if (this.viewCustContentKindList.ContainsKey(ID1) == true)
                                                {
                                                    find = true;
                                                    break;
                                                }
                                                else if (this.viewCustContentKindList.ContainsKey(ID2) == true)
                                                {
                                                    find = true;
                                                    break;
                                                }
                                            }
                                        }
                                    }
                                    if (find == false)
                                    {
                                        //ジャンル見つからないので除外
                                        continue;
                                    }
                                }
                                //イベントグループのチェック
                                if (eventInfo.EventGroupInfo != null)
                                {
                                    bool spanFlag = false;
                                    foreach (EpgEventData data in eventInfo.EventGroupInfo.eventDataList)
                                    {
                                        if (info.ServiceInfo.ONID == data.original_network_id &&
                                            info.ServiceInfo.TSID == data.transport_stream_id &&
                                            info.ServiceInfo.SID == data.service_id)
                                        {
                                            spanFlag = true;
                                            break;
                                        }
                                    }

                                    if (spanFlag == false)
                                    {
                                        //サービス２やサービス３の結合されるべきもの
                                        continue;
                                    }
                                }

                                SearchItem item = new SearchItem();
                                item.EventInfo = eventInfo;
                                item.ServiceName = info.ServiceInfo.service_name;

                                //予約チェック
                                foreach (ReserveData resInfo in CommonManager.Instance.DB.ReserveList.Values)
                                {
                                    if (CommonManager.EqualsPg(resInfo, eventInfo) == true)
                                    {
                                        item.ReserveInfo = resInfo;
                                        break;
                                    }
                                }

                                programList.Add(item);
                            }
                        }
                    }
                }
                //listView_event.DataContext = programList;
                listView_event.ItemsSource = programList;

                if (_lastHeaderClicked != null)
                {
                    //string header = ((Binding)_lastHeaderClicked.DisplayMemberBinding).Path.Path;
                    Sort(_lastHeaderClicked, _lastDirection);
                }
                else
                {
                    string header = ((Binding)gridView_event.Columns[1].DisplayMemberBinding).Path.Path;
                    Sort(header, _lastDirection);
                    _lastHeaderClicked = header;
                }

                //選択情報の復元
                RestoreListViewSelected(oldItem);//, oldItems);                
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.Message + "\r\n" + ex.StackTrace);
            }
        }

        private void CheckBox_Checked(object sender, RoutedEventArgs e)
        {
            UpdateEventList();
        }

        private void CheckBox_Unchecked(object sender, RoutedEventArgs e)
        {
            UpdateEventList();
        }

        private void button_chkAll_Click(object sender, RoutedEventArgs e)
        {
            listBox_service.ItemsSource = null;
            foreach (ServiceItem info in serviceList)
            {
                info.IsSelected = true;
            }
            listBox_service.ItemsSource = serviceList;
            UpdateEventList();
        }

        private void button_clearAll_Click(object sender, RoutedEventArgs e)
        {
            listBox_service.ItemsSource = null;
            foreach (ServiceItem info in serviceList)
            {
                info.IsSelected = false;
            }
            listBox_service.ItemsSource = serviceList;
            UpdateEventList();
        }

        private void listView_event_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            richTextBox_eventInfo.Document.Blocks.Clear();
            scrollViewer1.ScrollToHome();
            if (listView_event.SelectedItem != null)
            {
                SearchItem item = listView_event.SelectedItem as SearchItem;
                EpgEventInfo eventInfo = item.EventInfo;
                richTextBox_eventInfo.Document = CommonManager.Instance.ConvertDisplayText(eventInfo);
            }
        }

        private void listView_event_MouseDoubleClick(object sender, MouseButtonEventArgs e)
        {
            try
            {
                if (listView_event.SelectedItem != null)
                {
                    SearchItem item = listView_event.SelectedItem as SearchItem;
                    if (item.IsReserved == true)
                    {
                        ChangeReserve(item.ReserveInfo);
                    }
                    else
                    {
                        AddReserve(item.EventInfo);
                    }
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.Message + "\r\n" + ex.StackTrace);
            }
        }

        private void ChangeReserve(ReserveData reserveInfo)
        {
            try{
                ChgReserveWindow dlg = new ChgReserveWindow();
                dlg.Owner = (Window)PresentationSource.FromVisual(this).RootVisual;
                dlg.SetOpenMode(Settings.Instance.EpgInfoOpenMode);
                dlg.SetReserveInfo(reserveInfo);
                if (dlg.ShowDialog() == true)
                {
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.Message + "\r\n" + ex.StackTrace);
            }
        }

        private void AddReserve(EpgEventInfo eventInfo)
        {
            try
            {
                AddReserveEpgWindow dlg = new AddReserveEpgWindow();
                dlg.Owner = (Window)PresentationSource.FromVisual(this).RootVisual;
                dlg.SetOpenMode(Settings.Instance.EpgInfoOpenMode);
                dlg.SetEventInfo(eventInfo);
                if (dlg.ShowDialog() == true)
                {
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.Message + "\r\n" + ex.StackTrace);
            }
        }

        private void listView_event_ContextMenuOpening(object sender, ContextMenuEventArgs e)
        {
            if (sender.GetType() == typeof(ListView))
            {
                try
                {
                    cm_chg_viewMode3.IsChecked = true;

                    if (listView_event.SelectedItem != null)
                    {
                        SearchItem item = listView_event.SelectedItem as SearchItem;
                        if (item.IsReserved == true)
                        {
                            cm_Reverse.Header = "予約←→無効";
                            cm_del.IsEnabled = true;
                            cm_chg.IsEnabled = true;
                            for (int i = 0; i <= 5; i++)
                            {
                                ((MenuItem)cm_chg.Items[cm_chg.Items.IndexOf(recmode_all) + i]).IsChecked = (i == item.ReserveInfo.RecSetting.RecMode);
                            }
                            for (int i = 0; i < cm_pri.Items.Count; i++)
                            {
                                ((MenuItem)cm_pri.Items[i]).IsChecked = (i + 1 == item.ReserveInfo.RecSetting.Priority);
                            }
                            cm_pri.Header = string.Format((string)cm_pri.Tag, item.ReserveInfo.RecSetting.Priority);
                            cm_add.IsEnabled = false;
                            cm_autoadd.IsEnabled = true;
                            cm_timeshift.IsEnabled = true;
                        }
                        else
                        {
                            cm_Reverse.Header = "簡易予約";
                            cm_del.IsEnabled = false;
                            cm_chg.IsEnabled = false;
                            cm_add.IsEnabled = true;
                            cm_autoadd.IsEnabled = true;
                            cm_timeshift.IsEnabled = false;
                            cm_add_preset.Items.Clear();

                            foreach (RecPresetItem info in Settings.Instance.RecPresetList)
                            {
                                MenuItem menuItem = new MenuItem();
                                menuItem.Header = info.DisplayName;
                                menuItem.DataContext = info.ID;
                                menuItem.Click += new RoutedEventHandler(cm_add_preset_Click);

                                cm_add_preset.Items.Add(menuItem);
                            }
                        }

                        cm_autoadd.ToolTip = CommonManager.Instance.MUtil.EpgKeyword_TrimMode();
                        cm_CopyTitle.ToolTip = CommonManager.Instance.MUtil.CopyTitle_TrimMode();
                        cm_CopyContent.ToolTip = CommonManager.Instance.MUtil.CopyContent_Mode();
                        cm_SearchTitle.ToolTip = CommonManager.Instance.MUtil.SearchText_TrimMode();

                        cm_CmAppend.Visibility = System.Windows.Visibility.Collapsed;
                        cm_CopyTitle.Visibility = System.Windows.Visibility.Collapsed;
                        cm_CopyContent.Visibility = System.Windows.Visibility.Collapsed;
                        cm_SearchTitle.Visibility = System.Windows.Visibility.Collapsed;
                        if (Settings.Instance.CmAppendMenu == true)
                        {
                            cm_CmAppend.Visibility = System.Windows.Visibility.Visible;
                            if (Settings.Instance.CmCopyTitle == true)
                            {
                                cm_CopyTitle.Visibility = System.Windows.Visibility.Visible;
                            }
                            if (Settings.Instance.CmCopyContent == true)
                            {
                                cm_CopyContent.Visibility = System.Windows.Visibility.Visible;
                            }
                            if (Settings.Instance.CmSearchTitle == true)
                            {
                                cm_SearchTitle.Visibility = System.Windows.Visibility.Visible;
                            }
                        }

                    }
                }
                catch (Exception ex)
                {
                    MessageBox.Show(ex.Message + "\r\n" + ex.StackTrace);
                }
            }
        }

        void listView_event_PreviewKeyDown(object sender, KeyEventArgs e)
        {
            if (Keyboard.Modifiers == ModifierKeys.Control)
            {
                switch (e.Key)
                {
                    case Key.P:
                        this.cm_timeShiftPlay_Click(this.listView_event.SelectedItem, new RoutedEventArgs(Button.ClickEvent));
                        break;
                    case Key.D:
                        this.deleteItem();
                        break;
                    case Key.S:
                        this.cm_reverse_Click(this, new RoutedEventArgs(Button.ClickEvent));
                        break;
                    case Key.C:
                        this.CopyTitle2Clipboard();
                        break;
                }
            }
            else if (Keyboard.Modifiers == ModifierKeys.None)
            {
                switch (e.Key)
                {
                    case Key.Enter:
                        if (listView_event.SelectedItem != null)
                        {
                            SearchItem item = listView_event.SelectedItem as SearchItem;
                            if (item.IsReserved == true)
                            {
                                ChangeReserve(item.ReserveInfo);
                            }
                            else
                            {
                                AddReserve(item.EventInfo);
                            }
                            e.Handled = true;
                        }
                        break;
                    case Key.Delete:
                        this.deleteItem();
                        e.Handled = true;
                        break;
                }
            }
        }

        void deleteItem()
        {
            if (listView_event.SelectedItem == null) { return; }
            //
            List<ReserveData> delList = new List<ReserveData>();
            foreach (SearchItem item in listView_event.SelectedItems)
            {
                if (item.IsReserved == true)
                {
                    delList.Add(item.ReserveInfo);
                }
            }
            if (delList.Count == 0) { return; }

            string text1 = "削除しますか?　[削除アイテム数: " + delList.Count + "]" + "\r\n\r\n";
            foreach (ReserveData item in delList)
            {
                text1 += " ・ " + item.Title + "\r\n";
            }
            string caption1 = "登録項目削除の確認";
            if (MessageBox.Show(text1, caption1, MessageBoxButton.OKCancel, MessageBoxImage.Exclamation, MessageBoxResult.OK) == MessageBoxResult.OK)
            {
                this.cm_del_Click(this.listView_event.SelectedItem, new RoutedEventArgs(Button.ClickEvent));
            }
        }

        void cm_add_preset_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                if (sender.GetType() != typeof(MenuItem))
                {
                    return;
                }
                MenuItem meun = sender as MenuItem;
                UInt32 presetID = (UInt32)meun.DataContext;

                if (listView_event.SelectedItem == null)
                {
                    return;
                }

                SearchItem item = listView_event.SelectedItem as SearchItem;
                EpgEventInfo eventInfo = item.EventInfo;
                if (eventInfo.StartTimeFlag == 0)
                {
                    MessageBox.Show("開始時間未定のため予約できません");
                    return;
                }

                ReserveData reserveInfo = new ReserveData();
                CommonManager.ConvertEpgToReserveData(eventInfo, ref reserveInfo);

                RecSettingData setInfo = new RecSettingData();
                Settings.GetDefRecSetting(presetID, ref setInfo);
                reserveInfo.RecSetting = setInfo;

                List<ReserveData> list = new List<ReserveData>();
                list.Add(reserveInfo);
                ErrCode err = (ErrCode)cmd.SendAddReserve(list);
                if (err == ErrCode.CMD_ERR_CONNECT)
                {
                    MessageBox.Show("サーバー または EpgTimerSrv に接続できませんでした。");
                }
                if (err == ErrCode.CMD_ERR_TIMEOUT)
                {
                    MessageBox.Show("EpgTimerSrvとの接続にタイムアウトしました。");
                }
                if (err != ErrCode.CMD_SUCCESS)
                {
                    MessageBox.Show("予約登録でエラーが発生しました。終了時間がすでに過ぎている可能性があります。");
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.Message + "\r\n" + ex.StackTrace);
            }
        }

        private void cm_del_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                List<UInt32> list = new List<UInt32>();
                foreach(SearchItem item in listView_event.SelectedItems)
                {
                    if (item.IsReserved == true)
                    {
                        list.Add(item.ReserveInfo.ReserveID);
                    }
                }
                if (list.Count > 0)
                {
                    ErrCode err = (ErrCode)cmd.SendDelReserve(list);
                    if (err == ErrCode.CMD_ERR_CONNECT)
                    {
                        MessageBox.Show("サーバー または EpgTimerSrv に接続できませんでした。");
                    }
                    if (err == ErrCode.CMD_ERR_TIMEOUT)
                    {
                        MessageBox.Show("EpgTimerSrvとの接続にタイムアウトしました。");
                    }
                    if (err != ErrCode.CMD_SUCCESS)
                    {
                        MessageBox.Show("予約削除でエラーが発生しました。");
                    }
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.Message + "\r\n" + ex.StackTrace);
            }
        }

        private void cm_change_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                if (listView_event.SelectedItem != null)
                {
                    SearchItem item = listView_event.SelectedItem as SearchItem;
                    if (item.IsReserved == true)
                    {
                        ChangeReserve(item.ReserveInfo);
                    }
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.Message + "\r\n" + ex.StackTrace);
            }
        }

        private void cm_add_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                if (listView_event.SelectedItem != null)
                {
                    SearchItem item = listView_event.SelectedItem as SearchItem;
                    if (item.IsReserved == false)
                    {
                        AddReserve(item.EventInfo);
                    }
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.Message + "\r\n" + ex.StackTrace);
            }
        }

        private void cm_chg_recmode_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                List<ReserveData> list = new List<ReserveData>();
                foreach (SearchItem item in listView_event.SelectedItems)
                {
                    if (item.IsReserved == true)
                    {
                        ReserveData reserveInfo = item.ReserveInfo;

                        byte recMode = 0;
                        if (sender == recmode_all)
                        {
                            recMode = 0;
                        }
                        else if (sender == recmode_only)
                        {
                            recMode = 1;
                        }
                        else if (sender == recmode_all_nodec)
                        {
                            recMode = 2;
                        }
                        else if (sender == recmode_only_nodec)
                        {
                            recMode = 3;
                        }
                        else if (sender == recmode_view)
                        {
                            recMode = 4;
                        }
                        else if (sender == recmode_no)
                        {
                            recMode = 5;
                        }
                        else
                        {
                            return;
                        }
                        reserveInfo.RecSetting.RecMode = recMode;
                        
                        list.Add(reserveInfo);
                    }
                }
                if (list.Count > 0)
                {
                    ErrCode err = (ErrCode)cmd.SendChgReserve(list);
                    if (err == ErrCode.CMD_ERR_CONNECT)
                    {
                        MessageBox.Show("サーバー または EpgTimerSrv に接続できませんでした。");
                    }
                    if (err == ErrCode.CMD_ERR_TIMEOUT)
                    {
                        MessageBox.Show("EpgTimerSrvとの接続にタイムアウトしました。");
                    }
                    if (err != ErrCode.CMD_SUCCESS)
                    {
                        MessageBox.Show("予約変更でエラーが発生しました。");
                    }
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.Message + "\r\n" + ex.StackTrace);
            }
        }

        private void cm_chg_priority_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                List<ReserveData> list = new List<ReserveData>();
                foreach (SearchItem item in listView_event.SelectedItems)
                {
                    if (item.IsReserved == true)
                    {
                        ReserveData reserveInfo = item.ReserveInfo;

                        byte priority = 1;
                        if (sender == priority_1)
                        {
                            priority = 1;
                        }
                        else if (sender == priority_2)
                        {
                            priority = 2;
                        }
                        else if (sender == priority_3)
                        {
                            priority = 3;
                        }
                        else if (sender == priority_4)
                        {
                            priority = 4;
                        }
                        else if (sender == priority_5)
                        {
                            priority = 5;
                        }
                        else
                        {
                            return;
                        }
                        reserveInfo.RecSetting.Priority = priority;

                        list.Add(reserveInfo);
                    }
                }
                if (list.Count > 0)
                {
                    ErrCode err = (ErrCode)cmd.SendChgReserve(list);
                    if (err == ErrCode.CMD_ERR_CONNECT)
                    {
                        MessageBox.Show("サーバー または EpgTimerSrv に接続できませんでした。");
                    }
                    if (err == ErrCode.CMD_ERR_TIMEOUT)
                    {
                        MessageBox.Show("EpgTimerSrvとの接続にタイムアウトしました。");
                    }
                    if (err != ErrCode.CMD_SUCCESS)
                    {
                        MessageBox.Show("予約変更でエラーが発生しました。");
                    }
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.Message + "\r\n" + ex.StackTrace);
            }
        }

        private void cm_autoadd_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                if (sender.GetType() != typeof(MenuItem))
                {
                    return;
                }
                if (listView_event.SelectedItem != null)
                {
                    SearchItem item = listView_event.SelectedItem as SearchItem;

                    SearchWindow dlg = new SearchWindow();
                    dlg.Owner = (Window)PresentationSource.FromVisual(this).RootVisual;
                    dlg.SetViewMode(1);

                    EpgSearchKeyInfo key = new EpgSearchKeyInfo();

                    if (item.EventInfo.ShortInfo != null)
                    {
                        key.andKey = CommonManager.Instance.MUtil.TrimEpgKeyword(item.EventInfo.ShortInfo.event_name);
                    }
                    Int64 sidKey = ((Int64)item.EventInfo.original_network_id) << 32 | ((Int64)item.EventInfo.transport_stream_id) << 16 | ((Int64)item.EventInfo.service_id);
                    key.serviceList.Add(sidKey);

                    dlg.SetSearchDefKey(key);
                    dlg.ShowDialog();                
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.Message + "\r\n" + ex.StackTrace);
            }
        }

        private void cm_timeShiftPlay_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                if (listView_event.SelectedItem != null)
                {
                    SearchItem item = listView_event.SelectedItem as SearchItem;
                    if (item.IsReserved == true)
                    {
                        CommonManager.Instance.TVTestCtrl.StartTimeShift(item.ReserveInfo.ReserveID);
                    }
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.Message + "\r\n" + ex.StackTrace);
            }
        }

        /// <summary>
        /// 右クリックメニュー 表示設定イベント呼び出し
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void cm_viewSet_Click(object sender, RoutedEventArgs e)
        {
            if (ViewSettingClick != null)
            {
                ViewSettingClick(this, null);
            }
        }

        /// <summary>
        /// 右クリックメニュー 表示モードイベント呼び出し
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void cm_chg_viewMode_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                if (sender.GetType() != typeof(MenuItem))
                {
                    return;
                }
                if (ViewSettingClick != null)
                {
                    MenuItem item = sender as MenuItem;
                    CustomEpgTabInfo setInfo = new CustomEpgTabInfo();
                    setViewInfo.CopyTo(ref setInfo);
                    if (sender == cm_chg_viewMode2)
                    {
                        setInfo.ViewMode = 1;
                    }
                    else if (sender == cm_chg_viewMode3)
                    {
                        setInfo.ViewMode = 2;
                    }
                    else
                    {
                        setInfo.ViewMode = 0;
                    }
                    ViewSettingClick(this, setInfo);
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.Message + "\r\n" + ex.StackTrace);
            }
        }

        private void MenuItem_Click_CopyTitle(object sender, RoutedEventArgs e)
        {
            CopyTitle2Clipboard();
        }

        private void CopyTitle2Clipboard()
        {
            if (listView_event.SelectedItem != null)
            {
                SearchItem item = listView_event.SelectedItem as SearchItem;
                CommonManager.Instance.MUtil.CopyTitle2Clipboard(item.EventName);
            }
        }

        private void MenuItem_Click_CopyContent(object sender, RoutedEventArgs e)
        {
            if (listView_event.SelectedItem != null)
            {
                SearchItem item = listView_event.SelectedItems[listView_event.SelectedItems.Count - 1] as SearchItem;
                listView_event.UnselectAll();
                listView_event.SelectedItem = item;
                CommonManager.Instance.MUtil.CopyContent2Clipboard(item.EventInfo);
            }
        }

        private void MenuItem_Click_SearchTitle(object sender, RoutedEventArgs e)
        {
            if (listView_event.SelectedItem != null)
            {
                SearchItem item = listView_event.SelectedItems[listView_event.SelectedItems.Count - 1] as SearchItem;
                listView_event.UnselectAll();
                listView_event.SelectedItem = item;
                CommonManager.Instance.MUtil.SearchText(item.EventName);
            }
        }

        private void GridViewColumnHeader_Click(object sender, RoutedEventArgs e)
        {
            GridViewColumnHeader headerClicked = e.OriginalSource as GridViewColumnHeader;
            ListSortDirection direction;

            if (headerClicked != null)
            {
                if (headerClicked.Role != GridViewColumnHeaderRole.Padding)
                {
                    string header = "";
                    if (headerClicked.Column.DisplayMemberBinding != null)
                    {
                        header = ((Binding)headerClicked.Column.DisplayMemberBinding).Path.Path;
                    }
                    else if (headerClicked.Tag as string != null)
                    {
                        header = headerClicked.Tag as string;
                    }
                    if (header == null || header == "")
                    {
                        return;
                    }

                    if (String.Compare(header, _lastHeaderClicked) != 0)
                    {
                        direction = ListSortDirection.Ascending;
                        _lastHeaderClicked2 = _lastHeaderClicked;
                        _lastDirection2 = _lastDirection;
                    }
                    else
                    {
                        if (_lastDirection == ListSortDirection.Ascending)
                        {
                            direction = ListSortDirection.Descending;
                        }
                        else
                        {
                            direction = ListSortDirection.Ascending;
                        }
                    }

                    Sort(header, direction);

                    _lastHeaderClicked = header;
                    _lastDirection = direction;
                }
            }
        }

        private void Sort(string sortBy, ListSortDirection direction)
        {
            try
            {
                ICollectionView dataView = CollectionViewSource.GetDefaultView(listView_event.ItemsSource);

                dataView.SortDescriptions.Clear();

                SortDescription sd = new SortDescription(sortBy, direction);
                dataView.SortDescriptions.Add(sd);
                if (_lastHeaderClicked2 != null)
                {
                    if (String.Compare(sortBy, _lastHeaderClicked2) != 0)
                    {
                        SortDescription sd2 = new SortDescription(_lastHeaderClicked2, _lastDirection2);
                        dataView.SortDescriptions.Add(sd2);
                    }
                }
                dataView.Refresh();

                //Settings.Instance.ResColumnHead = sortBy;
                //Settings.Instance.ResSortDirection = direction;

            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.Message + "\r\n" + ex.StackTrace);
            }
        }

        /// <summary>
        /// 右クリックメニュー 簡易予約イベント呼び出し
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void cm_new_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                foreach (SearchItem item in listView_event.SelectedItems){
                    if (item.IsReserved == false)
                    {
                        EpgEventInfo eventInfo = item.EventInfo;

                        if (eventInfo.StartTimeFlag == 0)
                        {
                            MessageBox.Show("開始時間未定のため予約できません");
                            return;
                        }

                        ReserveData reserveInfo = new ReserveData();
                        CommonManager.ConvertEpgToReserveData(eventInfo, ref reserveInfo);

                        RecSettingData setInfo = new RecSettingData();
                        Settings.GetDefRecSetting(0, ref setInfo);  //  デフォルトをとって来てくれる？
                        reserveInfo.RecSetting = setInfo;

                        List<ReserveData> list = new List<ReserveData>();
                        list.Add(reserveInfo);
                        ErrCode err = (ErrCode)cmd.SendAddReserve(list);
                        if (err == ErrCode.CMD_ERR_CONNECT)
                        {
                            MessageBox.Show("サーバー または EpgTimerSrv に接続できませんでした。");
                        }
                        if (err == ErrCode.CMD_ERR_TIMEOUT)
                        {
                            MessageBox.Show("EpgTimerSrvとの接続にタイムアウトしました。");
                        }
                        if (err != ErrCode.CMD_SUCCESS)
                        {
                            MessageBox.Show("簡易予約でエラーが発生しました。終了時間がすでに過ぎている可能性があります。");
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.Message + "\r\n" + ex.StackTrace);
            }
        }

        /// <summary>
        /// 右クリックメニュー 予約←→無効クリックイベント呼び出し
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void cm_reverse_Click(object sender, RoutedEventArgs e)
        {

            if (listView_event.SelectedItem == null) return;

            List<ReserveData> list = new List<ReserveData>();
            bool IsExistNewReserve = false;

            foreach (SearchItem item in listView_event.SelectedItems)
            {
                if (item.IsReserved == true)
                {
                    if (item.ReserveInfo.RecSetting.RecMode == 5)
                    {
                        // 無効 => 予約
                        RecSettingData defSet = new RecSettingData();
                        Settings.GetDefRecSetting(0, ref defSet);
                        item.ReserveInfo.RecSetting.RecMode = defSet.RecMode;
                    }
                    else
                    {
                        //予約 => 無効
                        item.ReserveInfo.RecSetting.RecMode = 5;
                    }

                    list.Add(item.ReserveInfo);
                }
                else
                {
                    IsExistNewReserve = true;
                }
            }

            if (list.Count > 0)
            {
                try
                {
                    ErrCode err = (ErrCode)cmd.SendChgReserve(list);

                    if (err == ErrCode.CMD_ERR_CONNECT)
                    {
                        MessageBox.Show("サーバー または EpgTimerSrv に接続できませんでした。");
                    }
                    if (err == ErrCode.CMD_ERR_TIMEOUT)
                    {
                        MessageBox.Show("EpgTimerSrvとの接続にタイムアウトしました。");
                    }
                    if (err != ErrCode.CMD_SUCCESS)
                    {
                        MessageBox.Show("予約←→無効でエラーが発生しました。");
                    }
                }
                catch (Exception ex)
                {
                    MessageBox.Show(ex.Message + "\r\n" + ex.StackTrace);
                }
            }

            if (IsExistNewReserve == true)
            {
                cm_new_Click(sender, e);
            }

        }

        private void StoreListViewSelected(ref EpgEventInfo oldItem)//, ref List<EpgEventInfo> oldItems)
        {
            if (listView_event.SelectedItem != null)
            {
                oldItem = (listView_event.SelectedItem as SearchItem).EventInfo;
//                foreach (SearchItem item in listView_event.SelectedItems)
//                {
//                    oldItems.Add(item.EventInfo);
//                }
            }
        }

        private void RestoreListViewSelected(EpgEventInfo oldItem)//, List<EpgEventInfo> oldItems)
        {
            if (oldItem != null)
            {
                //このUnselectAll()は無いと正しく復元出来ない状況があり得る
                listView_event.UnselectAll();

                foreach (SearchItem item in listView_event.Items)
                {
                    if (CommonManager.EqualsPg(item.EventInfo, oldItem) == true)
                    {
                        listView_event.SelectedItem = item;
                        listView_event.ScrollIntoView(item);
                    }
                }

//                foreach (EpgEventInfo oldItem1 in oldItems)
//                {
//                    foreach (SearchItem item in listView_event.Items)
//                    {
//                        if (CommonManager.EqualsPg(item.EventInfo, oldItem1) == true)
//                        {
//                            listView_event.SelectedItems.Add(item);
//                            break;
//                        }
//                    }
//                }
            }
        }
        
        private void UserControl_IsVisibleChanged(object sender, DependencyPropertyChangedEventArgs e)
        {
            if (this.IsVisible == false) { return; }

            EpgEventInfo selectedItem = null;
            if (BlackoutWindow.selectedReserveItem != null)
            {
                selectedItem = BlackoutWindow.selectedReserveItem.EventInfo;
                BlackoutWindow.selectedReserveItem = null;
            }
            else if (BlackoutWindow.selectedSearchItem != null)
            {
                selectedItem = BlackoutWindow.selectedSearchItem.EventInfo;
                BlackoutWindow.selectedSearchItem = null;
            }

            if (selectedItem != null)
            {
                foreach (SearchItem item in listView_event.Items)
                {
                    if (CommonManager.EqualsPg(selectedItem, item.EventInfo) == true)
                    {
                        listView_event.SelectedItem = item;
                        listView_event.ScrollIntoView(item);
                        //画面更新されないので無意味
                        //listView_event.ScrollIntoView(listView_event.Items[0]);
                        //listView_event.ScrollIntoView(listView_event.Items[listView_event.Items.Count-1]);
                        //int scrollpos = ((listView_event.SelectedIndex - 5) >=0 ? scrollpos : 0);
                        //listView_event.ScrollIntoView(listView_event.Items[scrollpos]);
                        break;
                    }
                }
            }

        }

    }
}
