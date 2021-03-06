﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Linq.Expressions;
using System.Text;
using System.Text.RegularExpressions;
using System.Windows;
using System.Windows.Input;
using System.Windows.Controls;

using CtrlCmdCLI;
using CtrlCmdCLI.Def;

namespace EpgTimer
{
    public class MenuUtil
    {
        private CtrlCmdUtil cmd = null;

        public MenuUtil(CtrlCmdUtil ctrlCmd)
        {
            cmd = ctrlCmd;
        }

        public string TrimEpgKeyword(string txtKey, bool NotToggle = false)//NotToggleはショートカット用
        {
            string txtKey1 = txtKey;
            bool setting = Settings.Instance.MenuSet.Keyword_Trim;
            if (Keyboard.Modifiers == ModifierKeys.Shift && NotToggle == false)
            {
                setting=!setting;
            }

            if (setting == true)
            {
                txtKey1 = TrimKeyword(txtKey1);
            }

            return txtKey1;
        }

        public string EpgKeyword_TrimMode()
        {
            return TrimModeTooltip(Settings.Instance.MenuSet.Keyword_Trim);
        }

        private string TrimModeTooltip(bool mode)
        {
            string str_mode = (mode == true ? "オン" : "オフ");
            string str_mode_toggle = (mode == false ? "'オン'" : "'オフ'");
            return "記号除去モード : " + str_mode + " (Shift+クリックで一時的に" + str_mode_toggle + ")";
        }

        public void CopyTitle2Clipboard(string txtTitle, bool NotToggle = false)
        {
            string txtTitle1 = txtTitle;
            bool setting = Settings.Instance.MenuSet.CopyTitle_Trim;
            if (Keyboard.Modifiers == ModifierKeys.Shift && NotToggle == false)
            {
                setting = !setting;
            }

            if (setting == true)
            {
                txtTitle1 = TrimKeyword(txtTitle1);
            }

            Clipboard.SetDataObject(txtTitle1, true);
        }

        public string CopyTitle_TrimMode()
        {
            return TrimModeTooltip(Settings.Instance.MenuSet.CopyTitle_Trim);
        }

        public void CopyContent2Clipboard(EpgEventInfo eventInfo, bool NotToggle = false)
        {
            string text = "";

            if (eventInfo != null)
            {
                bool setting = Settings.Instance.MenuSet.CopyContentBasic;
                if (Keyboard.Modifiers == ModifierKeys.Shift && NotToggle == false)
                {
                    setting = !setting;
                }

                if (setting == true)
                {
                    //text = eventInfo.ShortInfo.text_char;
                    text = CommonManager.Instance.ConvertProgramText(eventInfo, EventInfoTextMode.BasicOnly);
                }
                else
                {
                    text = CommonManager.Instance.ConvertProgramText(eventInfo, EventInfoTextMode.All);
                }

                text = text.TrimEnd() + "\r\n";
            }

            Clipboard.SetDataObject(text, true);
        }

        public void CopyContent2Clipboard(ReserveData resInfo, bool NotToggle = false)
        {
            CopyContent2Clipboard(CommonManager.Instance.GetEpgEventInfoFromReserveData(resInfo, true), NotToggle);
        }

        public void CopyContent2Clipboard(RecFileInfo recInfo, bool NotToggle = false)
        {
            string text = "";

            if (recInfo != null)
            {
                bool setting = Settings.Instance.MenuSet.CopyContentBasic;
                if (Keyboard.Modifiers == ModifierKeys.Shift && NotToggle == false)
                {
                    setting = !setting;
                }

                if (setting == true)
                {
                    string[] stArrayData = recInfo.ProgramInfo.Replace("\r\n", "\n").Split('\n');
                    int endI = Math.Min(stArrayData.Length, 3);

                    for (int i = 0; i < endI; i++)
                    {
                        text += stArrayData[i] + "\r\n";
                    }
                }
                else
                {
                    text = recInfo.ProgramInfo;
                }

                text = text.TrimEnd() + "\r\n";
            }

            Clipboard.SetDataObject(text, true);
        }

        public string CopyContent_Mode()
        {
            string mode = (Settings.Instance.MenuSet.CopyContentBasic == true ? "基本情報のみ" : "詳細情報");
            string mode_toggle = (Settings.Instance.MenuSet.CopyContentBasic == false ? "'基本情報のみ'" : "'詳細情報'");
            return "取得モード : " + mode + " (Shift+クリックで一時的に" + mode_toggle + ")";
        }

        public void SearchText(string txtKey, bool NotToggle = false)
        {
            string txtKey1 = txtKey;
            bool setting = Settings.Instance.MenuSet.SearchTitle_Trim;
            if (Keyboard.Modifiers == ModifierKeys.Shift && NotToggle == false)
            {
                setting = !setting;
            }

            if (setting == true)
            {
                txtKey1 = TrimKeyword(txtKey1);
            }

            string txtURI = Settings.Instance.MenuSet.SearchURI;
            txtURI += UrlEncode(txtKey1, System.Text.Encoding.UTF8);

            try
            {
                System.Diagnostics.Process.Start(txtURI);
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.Message + "\r\n" + ex.StackTrace);
                MessageBox.Show("'検索のURI'の設定を確認してください。");
            }
        }

        public string SearchText_TrimMode()
        {
            return TrimModeTooltip(Settings.Instance.MenuSet.SearchTitle_Trim);
        }

        public string TrimKeyword(string txtKey)
        {
            //
            // 前後の記号を取り除く
            //
            // [二][字]NHKニュース7
            // ５．１[SS][字]NHK歌謡コンサート「人生の旅路に　この歌を」
            // 午後のロードショー「私がウォシャウスキー」魅惑のヒロイン特集[字][S][二]
            // 【Ｍｏｔｈｅｒ’ｓ　Ｄａｙ　Ｓｐｅｃｉａｌ】【映】バンガー・シスターズ
            string markExp1 =
                "(" +
                    "(\\[[^\\]]+\\])+" +
                    "|" +
                    "(【[^】]+】)+" +
                    "|" +
                    "(［[^］]+］)+" +
                ")";
            string[] exp = {
                                "^((５．１)|(5.1)|" + markExp1 + ")+", // 先頭の記号
                                "^(([#＃][\\d０-９]+)|(第[\\d０-９]+話))", // 先頭にある話数の除去
                                "^[\\(（][\\d０-９]+[\\)）]\\s*「[^」]+」", // 先頭にある話数の除去
                                "(([#＃][\\d０-９]+)|(第[\\d０-９]+話)).*", // ドラマ等の話数から後ろ全て
                                "[\\(（][\\d０-９]+[\\)）]\\s*「[^」]+」.*", // ドラマ等の話数から後ろ全て その2
                                //"「[^」]+」.*", // ドラマ等のサブタイトルから後ろ全て、ちょっと強すぎるかも
                                "<[^>]+>", // NHK・フジテレビが使う補足
                                "＜[^＞]+＞", // NHK・フジテレビが使う補足
                                "[◆▽].*", // タイトルに埋め込まれた番組説明
                                "[\\[［【\\(（][^\\]］】\\)）]*リマスター[^\\]］】\\)）]*[\\]］】\\)）]",// ときどき見かけるので1
                                "(((HD)|(ＨＤ)|(ハイビジョン)|(デジタル))リマスター((HD)|(ＨＤ))?版?)|(リマスター((HD)|(ＨＤ)|版)版?)",// 同上、括弧無しは特定パタンのみ
                                markExp1 + "$" // 末尾の記号
                                };
            foreach (string str1 in exp)
            {
                txtKey = Regex.Replace(txtKey, str1, string.Empty).Trim();
            }
            // 映画のタイトル抽出
            // TODO:正規表現を設定ファイルで変更可能にする
            string[] titleExp = {
                                "^「(?<Title>[^」]+)」",
                                "((サタ☆シネ)|(シネマズ?)|(シアター)|(プレミア)|(ロードショー)|(ロードSHOW!)|(午後ロード)|(木曜デラックス)|(映画天国))\\s*「(?<Title>[^」]+)」",
                                "((シネマ)|(キネマ)).*『(?<Title>[^』]+)』"
                                };
            foreach (string str1 in titleExp)
            {
                Match m = Regex.Match(txtKey, str1);
                if (m.Success == true)
                {
                    txtKey = m.Groups["Title"].Value;
                    break;
                }
            }

            return txtKey;
        }

        //
        // HttpUtility を使わないUrlEncodeの実装
        // From http://d.hatena.ne.jp/kazuv3/20080605/1212656674
        //
        public string UrlEncode(string s, System.Text.Encoding enc)
        {
            System.Text.StringBuilder rt = new System.Text.StringBuilder();
            foreach (byte i in enc.GetBytes(s))
                if (i == 0x20)
                    rt.Append('+');
                else if (i >= 0x30 && i <= 0x39 || i >= 0x41 && i <= 0x5a || i >= 0x61 && i <= 0x7a)
                    rt.Append((char)i);
                else
                    rt.Append("%" + i.ToString("X2"));
            return rt.ToString();
        }

        public string MarginText(RecSettingData recSetting, bool start)
        {
            return CustomTimeFormat(GetMargin(recSetting, start) * (start ? -1 : 1), recSetting.UseMargineFlag);
        }

        public int GetMargin(RecSettingData recSetting, bool start)
        {
            if (recSetting == null) return 0;

            int marginTime;
            if (recSetting.UseMargineFlag == 1)
            {
                marginTime = start ? recSetting.StartMargine : recSetting.EndMargine;
            }
            else
            {
                marginTime = IniFileHandler.GetPrivateProfileInt("SET", start ? "StartMargin" : "EndMargin", 0, SettingPath.TimerSrvIniPath);
            }
            return marginTime;
        }

        private string CustomTimeFormat(int span, byte useMarginFlag)
        {
            string hours;
            string minutes;
            string seconds = (span % 60).ToString("00;00");
            if (Math.Abs(span) < 3600)
            {
                hours = "";
                minutes = (span / 60).ToString("0;0") + ":";
            }
            else
            {
                hours = (span / 3600).ToString("0;0") + ":";
                minutes = ((span % 3600) / 60).ToString("00;00") + ":";
            }
            return span.ToString("+;-") + hours + minutes + seconds + (useMarginFlag == 1 ? " " : "*");
        }

        public TextBlock GetTooltipBlockStandard(string text)
        {
            TextBlock block = new TextBlock();
            block.Text = text;
            block.MaxWidth = 400;
            block.TextWrapping = TextWrapping.Wrap;
            return block;
        }

        public List<T> ToList<T>(T item)
        {
            return new List<T> { item };
        }

        /// <summary>メンバ名を返す。</summary>
        public string GetMemberName<T>(Expression<Func<T>> e)
        {
            var member = (MemberExpression)e.Body;
            return member.Member.Name;
        }
        
        /// <summary>
        /// 変換エラーの場合、デフォルト値を返し、テキストボックスの内容をデフォルト値に置き換える。
        /// </summary>
        public T MyToNumerical<T>(TextBox box, Func<string, T> converter, T defValue = default(T))
        {
            try
            {
                return converter(box.Text.ToString());
            }
            catch
            {
                box.Text = defValue.ToString();
                return defValue;
            }
        }
        
        public bool ReserveAdd(List<EpgEventInfo> itemlist, RecSettingView recSettingView, uint presetID = 0, bool cautionMany = true)
        {
            try
            {
                if (itemlist.Count == 1)
                {
                    if (IsEnableReserveAdd(itemlist[0]) == false) return false;
                }

                var setInfo = new RecSettingData();
                if (recSettingView != null)
                {
                    //ダイアログからの予約、SearchWindowの簡易予約
                    recSettingView.GetRecSetting(ref setInfo);
                }
                else
                {
                    //簡易予約やプリセット予約
                    Settings.GetDefRecSetting(presetID, ref setInfo);
                }

                var list = new List<ReserveData>();

                foreach (EpgEventInfo item in itemlist)
                {
                    if (item.StartTimeFlag != 0)
                    {
                        var resInfo = new ReserveData();
                        CommonManager.ConvertEpgToReserveData(item, ref resInfo);
                        resInfo.RecSetting = setInfo;
                        list.Add(resInfo);
                    }
                }

                return ReserveAdd(list, cautionMany);
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.Message + "\r\n" + ex.StackTrace);
                return false;
            }
        }
        public bool IsEnableReserveAdd(EpgEventInfo item)
        {
            if (item == null) return false;

            bool retv = (item.StartTimeFlag != 0);
            if (retv == false)
            {
                MessageBox.Show("開始時間未定のため予約できません");
            }
            return retv;
        }
        public bool ReserveAdd(List<ReserveData> list, bool cautionMany = true)
        {
            return ReserveCmdSend(list, cmd.SendAddReserve, "予約追加", cautionMany);
        }

        public bool ReserveChangeOnOff(List<ReserveData> itemlist, RecSettingView recSettingView = null, bool cautionMany = true)
        {
            try
            {
                //無効から戻す録画モードの選択
                var setInfo = new RecSettingData();

                //現在の設定を読み込む。SearchWindowの場合だけ。
                if (recSettingView != null)
                {
                    recSettingView.GetRecSetting(ref setInfo);
                    
                    //現在の設定が無効で登録の場合は、デフォルトの設定を読み込みに行く
                    if (setInfo.RecMode == 5)
                    {
                        recSettingView = null;
                    }
                }
                //デフォルト設定を読み込む
                if (recSettingView == null)
                {
                    Settings.GetDefRecSetting(0, ref setInfo);
                }
                //デフォルトも無効で登録なら、指定サービスにする
                byte recMode = setInfo.RecMode != 5 ? setInfo.RecMode : (byte)1;

                //個別設定なので、ChangeRecmode()は不可。
                itemlist.ForEach(item => item.RecSetting.RecMode = (item.RecSetting.RecMode == 5 ? recMode : (byte)5));

                return ReserveChange(itemlist, cautionMany);
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.Message + "\r\n" + ex.StackTrace);
                return false;
            }
        }

        public bool ChangeOnPreset(List<RecSettingData> infoList, uint presetID)
        {
            try
            {
                infoList.ForEach(info => Settings.GetDefRecSetting(presetID, ref info));
                return true;
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.Message + "\r\n" + ex.StackTrace);
                return false;
            }
        }

        public bool ChangeRecmode(List<RecSettingData> infoList, byte recMode)
        {
            try
            {
                infoList.ForEach(info => info.RecMode = recMode);
                return true;
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.Message + "\r\n" + ex.StackTrace);
                return false;
            }
        }

        public bool ChangePriority(List<RecSettingData> infoList, byte priority)
        {
            try
            {
                infoList.ForEach(info => info.Priority = priority);
                return true;
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.Message + "\r\n" + ex.StackTrace);
                return false;
            }
        }

        public bool ChangeRelay(List<RecSettingData> infoList, byte relay)
        {
            try
            {
                infoList.ForEach(info => info.TuijyuuFlag = relay);
                return true;
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.Message + "\r\n" + ex.StackTrace);
                return false;
            }
        }

        public bool ChangePittari(List<RecSettingData> infoList, byte pittari)
        {
            try
            {
                infoList.ForEach(info => info.PittariFlag = pittari);
                return true;
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.Message + "\r\n" + ex.StackTrace);
                return false;
            }
        }

        public bool ChangeMargin(List<RecSettingData> infoList, int margin_offset, bool start)
        {
            try
            {
                if (margin_offset == 0)
                {
                    infoList.ForEach(info => info.UseMargineFlag = 0);
                }
                else
                {
                    infoList.ForEach(info =>
                    {
                        info.UseMargineFlag = 1;
                        if (start == true)
                        {
                            info.StartMargine += margin_offset;
                        }
                        else
                        {
                            info.EndMargine += margin_offset;
                        }
                    });
                }

                return true;
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.Message + "\r\n" + ex.StackTrace);
                return false;
            }
        }

        public bool ChangeMarginValue(List<RecSettingData> infoList, bool start, Control owner = null)
        {
            try
            {
                infoList[0].UseMargineFlag = 1;

                Setting.SetDefRecSettingWindow dlg = new Setting.SetDefRecSettingWindow();
                dlg.Owner = (Window)PresentationSource.FromVisual(owner).RootVisual;
                dlg.SetSettingMode(start == true ? "開始マージン設定" : "終了マージン設定", 1);
                dlg.recSettingView.SetDefSetting(infoList[0]);
                dlg.recSettingView.SetChangeMode(start == true ? 0 : 1);

                if (dlg.ShowDialog() == false) return false;

                var setData = new RecSettingData();
                dlg.recSettingView.GetRecSetting(ref setData);

                infoList.ForEach(info =>
                {
                    info.UseMargineFlag = 1;
                    if (start == true)
                    {
                        info.StartMargine = setData.StartMargine;
                    }
                    else
                    {
                        info.EndMargine = setData.EndMargine;
                    }
                });
                return true;
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.Message + "\r\n" + ex.StackTrace);
                return false;
            }
        }

        public bool ChangeTuner(List<RecSettingData> infoList, uint tuner)
        {
            try
            {
                infoList.ForEach(info => info.TunerID = tuner);
                return true;
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.Message + "\r\n" + ex.StackTrace);
                return false;
            }
        }

        public bool ChangeBulkSet(List<RecSettingData> infoList, Control owner = null, bool pgAll = false)
        {
            try
            {
                Setting.SetDefRecSettingWindow dlg = new Setting.SetDefRecSettingWindow();
                dlg.Owner = (Window)PresentationSource.FromVisual(owner).RootVisual;
                dlg.SetSettingMode("まとめて変更");
                dlg.recSettingView.SetDefSetting(infoList[0]);
                if (pgAll == true)
                {
                    dlg.recSettingView.SetViewMode(false);
                }

                if (dlg.ShowDialog() == false) return false;

                var setData = new RecSettingData();
                dlg.recSettingView.GetRecSetting(ref setData);

                infoList.ForEach(info => setData.CopyTo(info));
                return true;
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.Message + "\r\n" + ex.StackTrace);
                return false;
            }
        }

        public bool ChgGenre(List<EpgSearchKeyInfo> infoList, Control owner = null)
        {
            try
            {
                SetDefSearchSettingWindow dlg = new SetDefSearchSettingWindow();
                dlg.Owner = (Window)PresentationSource.FromVisual(owner).RootVisual;
                dlg.SetDefSetting(infoList[0]);
                dlg.searchKey.searchKeyDescView.SetChangeMode(0);

                if (dlg.ShowDialog() == false) return false;

                var setData = new EpgSearchKeyInfo();
                dlg.GetSetting(ref setData);

                infoList.ForEach(info => info.contentList = setData.contentList.Clone());
                return true;
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.Message + "\r\n" + ex.StackTrace);
                return false;
            }
        }

        public bool ReserveChange(List<ReserveData> itemlist, bool cautionMany = true)
        {
            return ReserveCmdSend(itemlist, cmd.SendChgReserve, "予約変更", cautionMany);
        }

        public bool ReserveDelete(List<ReserveData> itemlist)
        {
            try
            {
                List<uint> list = itemlist.Select(item => item.ReserveID).ToList();
                return ReserveCmdSend(list, cmd.SendDelReserve, "予約削除");
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.Message + "\r\n" + ex.StackTrace);
                return false;
            }
        }

        public bool EpgAutoAddAdd(List<EpgAutoAddData> itemlist)
        {
            return ReserveCmdSend(itemlist, cmd.SendAddEpgAutoAdd, "EPG自動予約の追加");
        }
        public bool EpgAutoAddChangeNotKey(List<EpgAutoAddData> itemlist)
        {
            try
            {
                itemlist.ForEach(item => item.searchInfo.notKey = Clipboard.GetText());
                return ReserveCmdSend(itemlist, cmd.SendChgEpgAutoAdd, "EPG自動予約の変更");
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.Message + "\r\n" + ex.StackTrace);
                return false;
            }
        }
        public bool EpgAutoAddChange(List<EpgAutoAddData> itemlist, bool cautionMany = true)
        {
            return ReserveCmdSend(itemlist, cmd.SendChgEpgAutoAdd, "EPG自動予約の変更", cautionMany);
        }
        public bool EpgAutoAddDelete(List<EpgAutoAddData> itemlist)
        {
            try
            {
                List<uint> list = itemlist.Select(item => item.dataID).ToList();
                return EpgAutoAddDelete(list);
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.Message + "\r\n" + ex.StackTrace);
                return false;
            }
        }
        public bool EpgAutoAddDelete(List<uint> itemlist)
        {
            return ReserveCmdSend(itemlist, cmd.SendDelEpgAutoAdd, "プログラム自動予約の削除");
        }

        public bool ManualAutoAddAdd(List<ManualAutoAddData> itemlist)
        {
            return ReserveCmdSend(itemlist, cmd.SendAddManualAdd, "プログラム自動予約の追加");
        }
        public bool ManualAutoAddChange(List<ManualAutoAddData> itemlist, bool cautionMany = true)
        {
            return ReserveCmdSend(itemlist, cmd.SendChgManualAdd, "プログラム自動予約の変更", cautionMany);
        }
        public bool ManualAutoAddDelete(List<ManualAutoAddData> itemlist)
        {
            try
            {
                List<uint> list = itemlist.Select(item => item.dataID).ToList();
                return ReserveCmdSend(list, cmd.SendDelManualAdd, "プログラム自動予約の削除");
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.Message + "\r\n" + ex.StackTrace);
                return false;
            }
        }
        public bool RecinfoChgProtect(List<RecFileInfo> itemlist)
        {
            try
            {
                itemlist.ForEach(item => item.ProtectFlag = !item.ProtectFlag);
                return ReserveCmdSend(itemlist, cmd.SendChgProtectRecInfo, "録画情報の変更");
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.Message + "\r\n" + ex.StackTrace);
                return false;
            }
        }
        public bool RecinfoDelete(List<RecFileInfo> itemlist)
        {
            try
            {
                List<uint> list = itemlist.Select(item => item.ID).ToList();
                return ReserveCmdSend(list, cmd.SendDelRecInfo, "録画情報の削除");
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.Message + "\r\n" + ex.StackTrace);
                return false;
            }
        }

        private bool ReserveCmdSend<T>(List<T> list, Func<List<T>, uint> cmdSend, string description = "", bool cautionMany = true)
        {
            try
            {
                if (list.Count == 0) return true;

                if (cautionMany == true && CautionManyMessage(list.Count, description) == false) return false;

                ErrCode err = (ErrCode)cmdSend(list);
                return CommonManager.CmdErrMsgTypical(err, description);
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.Message + "\r\n" + ex.StackTrace);
                return false;
            }
        }
        public bool CautionManyMessage(int Count, string description = "")
        {
            if (Settings.Instance.CautionManyChange == true && Count >= Settings.Instance.CautionManyNum)
            {
                if (MessageBox.Show("多数の項目を処理しようとしています。\r\nよろしいですか？\r\n"
                    + "　項目数: " + Count + "\r\n"
                    , description, MessageBoxButton.OKCancel) == MessageBoxResult.Cancel)
                {
                    return false;
                }
            }
            return true;
        }

        public bool? OpenSearchItemWithDialog(SearchItem item, Control Owner, byte openMode = 0)
        {
            if (item == null) return null;

            if (item.IsReserved == true)
            {
                return OpenChangeReserveDialog(item.ReserveInfo, Owner, openMode);
            }
            else
            {
                return OpenEpgReserveDialog(item.EventInfo, Owner, openMode);
            }
        }

        public bool? OpenEpgReserveDialog(EpgEventInfo Data, Control Owner, byte epgInfoOpenMode = 0)
        {
            try
            {
                var dlg = new AddReserveEpgWindow();
                dlg.Owner = (Window)PresentationSource.FromVisual(Owner).RootVisual;
                dlg.SetEventInfo(Data);
                dlg.SetOpenMode(epgInfoOpenMode);
                return dlg.ShowDialog();
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.Message + "\r\n" + ex.StackTrace);
                return null;
            }
        }

        public bool? OpenChangeReserveDialog(ReserveData Data, Control Owner, byte epgInfoOpenMode = 0)
        {
            return OpenChgReserveDialog(Data, Owner, epgInfoOpenMode);
        }
        public bool? OpenManualReserveDialog(Control Owner)
        {
            return OpenChgReserveDialog(null, Owner);
        }
        public bool? OpenChgReserveDialog(ReserveData Data, Control Owner, byte epgInfoOpenMode = 0)
        {
            try
            {
                var dlg = new ChgReserveWindow();
                dlg.Owner = (Window)PresentationSource.FromVisual(Owner).RootVisual;
                if (Data != null)
                {
                    dlg.SetReserveInfo(Data);
                    dlg.SetOpenMode(epgInfoOpenMode);
                }
                else
                {
                    dlg.SetAddReserveMode();
                }
                return dlg.ShowDialog();
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.Message + "\r\n" + ex.StackTrace);
                return null;
            }
        }

        public bool? OpenSearchEpgDialog(Control Owner)
        {
            return OpenEpgAutoAddDialog(null, Owner, SearchWindow.SearchMode.Find);
        }
        public bool? OpenAddEpgAutoAddDialog(Control Owner)
        {
            return OpenEpgAutoAddDialog(null, Owner, SearchWindow.SearchMode.NewAdd);
        }
        public bool? OpenChangeEpgAutoAddDialog(EpgAutoAddData Data, Control Owner)
        {
            return OpenEpgAutoAddDialog(Data, Owner, SearchWindow.SearchMode.Change);
        }
        private bool? OpenEpgAutoAddDialog(EpgAutoAddData Data, Control Owner, SearchWindow.SearchMode mode)
        {
            try
            {
                var dlg = new SearchWindow();
                dlg.Owner = (Window)PresentationSource.FromVisual(Owner).RootVisual;
                dlg.SetViewMode(mode);
                if (Data != null)
                {
                    dlg.SetChgAutoAddID(Data.dataID);
                    dlg.SetSearchDefKey(Data.searchInfo);
                    dlg.SetRecInfoDef(Data.recSetting);
                }
                return dlg.ShowDialog();
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.Message + "\r\n" + ex.StackTrace);
                return null;
            }
        }
        public void SendAutoAdd(EpgEventInfo item, Control Owner, bool NotToggle = false)
        {
            SendAutoAdd(item.Title(), item.Create64Key(), Owner, NotToggle);
        }
        public void SendAutoAdd(RecFileInfo item, Control Owner, bool NotToggle = false)
        {
            SendAutoAdd(item.Title, item.Create64Key(), Owner, NotToggle);
        }
        public void SendAutoAdd(ReserveData item, Control Owner, bool NotToggle = false)
        {
            SendAutoAdd(item.Title, item.Create64Key(), Owner, NotToggle);
        }
        public void SendAutoAdd(string Title, UInt64 sidKey, Control Owner, bool NotToggle = false)
        {
            try
            {
                var dlg = new SearchWindow();
                dlg.Owner = (Window)PresentationSource.FromVisual(Owner).RootVisual;
                dlg.SetViewMode(SearchWindow.SearchMode.NewAdd);

                var key = new EpgSearchKeyInfo();
                key.andKey = TrimEpgKeyword(Title, NotToggle);
                key.serviceList.Add((Int64)sidKey);

                dlg.SetSearchDefKey(key);
                dlg.ShowDialog();
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.Message + "\r\n" + ex.StackTrace);
            }
        }

        public bool? OpenAddManualAutoAddDialog(Control Owner)
        {
            return OpenManualAutoAddDialog(null, Owner);
        }
        public bool? OpenChangeManualAutoAddDialog(ManualAutoAddData Data, Control Owner)
        {
            return OpenManualAutoAddDialog(Data, Owner);
        }
        public bool? OpenManualAutoAddDialog(ManualAutoAddData Data, Control Owner)
        {
            try
            {
                var dlg = new AddManualAutoAddWindow();
                dlg.Owner = (Window)PresentationSource.FromVisual(Owner).RootVisual;
                if (Data != null)
                {
                    dlg.SetChangeMode(true);
                    dlg.SetDefaultSetting(Data);
                }
                return dlg.ShowDialog();
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.Message + "\r\n" + ex.StackTrace);
                return null;
            }
        }

        public bool? OpenRecInfoDialog(RecFileInfo info, Control Owner)
        {
            try
            {
                var dlg = new RecInfoDescWindow();
                dlg.Owner = (Window)PresentationSource.FromVisual(Owner).RootVisual;
                dlg.SetRecInfo(info);
                return dlg.ShowDialog();
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.Message + "\r\n" + ex.StackTrace);
                return null;
            }
        }

        public void SetSearchItemReserved(ICollection<SearchItem> list)
        {
            var listKeys = new Dictionary<ulong, SearchItem>();

            foreach (SearchItem listItem1 in list)
            {
                //重複するキーは基本的に無いという前提
                try
                {
                    listKeys.Add(listItem1.EventInfo.Create64PgKey(), listItem1);
                    listItem1.ReserveInfo = null;
                }
                catch { }
            }

            SearchItem setItem;
            foreach (ReserveData data in CommonManager.Instance.DB.ReserveList.Values)
            {
                if (listKeys.TryGetValue(data.Create64PgKey(), out setItem))
                {
                    setItem.ReserveInfo = data;
                }
            }
        }

        public EpgEventInfo SearchEventLikeThat(ReserveData resInfo)
        {
            if (resInfo == null) return null;
            double dist = double.MaxValue, dist1;
            EpgEventInfo eventPossible = null;

            UInt64 key = resInfo.Create64Key();
            if (CommonManager.Instance.DB.ServiceEventList.ContainsKey(key) == true)
            {
                foreach (EpgEventInfo eventChkInfo in CommonManager.Instance.DB.ServiceEventList[key].eventList)
                {
                    dist1 = Math.Abs((resInfo.StartTime - eventChkInfo.start_time).TotalSeconds);
                    double overlapLength = CulcOverlapLength(resInfo.StartTime, resInfo.DurationSecond,
                                                            eventChkInfo.start_time, eventChkInfo.durationSec);

                    //開始時間が最も近いものを選ぶ。同じ差なら時間が前のものを選ぶ
                    if (overlapLength >= 0 && (dist > dist1 ||
                        dist == dist1 && (eventPossible == null || resInfo.StartTime > eventChkInfo.start_time)))
                    {
                        dist = dist1;
                        eventPossible = eventChkInfo;
                        if (dist == 0) break;
                    }
                }
            }

            return eventPossible;
        }

        private double CulcOverlapLength(DateTime s1, uint d1, DateTime s2, uint d2)
        {
            //重複してない場合は負数が返る。
            TimeSpan ts1 = s1 + TimeSpan.FromSeconds(d1) - s2;
            TimeSpan ts2 = s2 + TimeSpan.FromSeconds(d2) - s1;
            return Math.Min(Math.Min(ts1.TotalSeconds, ts2.TotalSeconds), Math.Min(d1, d2));
        }

        public void FilePlay(String filePath)
        {
            try
            {
                if (filePath == null || filePath.Length == 0) return;

                CommonManager cmg = CommonManager.Instance;
                if (cmg.NWMode == false)
                {
                    System.Diagnostics.Process process;
                    if (Settings.Instance.FilePlayExe.Length == 0)
                    {
                        process = System.Diagnostics.Process.Start(filePath);
                    }
                    else
                    {
                        String cmdLine = Settings.Instance.FilePlayCmd;
                        cmdLine = cmdLine.Replace("$FilePath$", filePath);
                        process = System.Diagnostics.Process.Start(Settings.Instance.FilePlayExe, cmdLine);
                    }
                }
                else
                {
                    cmg.TVTestCtrl.StartStreamingPlay(filePath, cmg.NW.ConnectedIP, cmg.NW.ConnectedPort);
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.Message + "\r\n" + ex.StackTrace);
            }
        }

    }

}
