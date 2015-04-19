EpgDataCap_Bon
==============
**BonDriver based multifunctional EPG software**

Documents are stored in the 'Document' directory.  
Configuration files are stored in the 'ini' directory.

**このフォークについて**

このフォークはxtne6fさんのフォークにちょびっとだけパッチを追加するブランチ(フォーク)です。  
ビルド方法などは[xtne6fさんのフォーク](https://github.com/xtne6f/EDCB)を参照してください。

EpgTimerの変更が中心ですが、あちこち変更しているのと、かなり微妙なコミット([ff60480](https://github.com/tkntrec/EDCB/commit/ff6048074a4a609fb22c78361682a3cb4cf4a593)とか)も混ざっていますので、ご注意を。  
なお、このフォークのブランチは再作成などで構成が変わることがあります。

[branch:my-ui](https://github.com/tkntrec/EDCB/tree/my-ui)
* EpgTimer側の差分です。少なくとも[branch:my-work](https://github.com/tkntrec/EDCB/tree/ct-rb3)をマージしないとビルド出来ません。
* 主に検索(自動予約登録)まわりで若干の機能追加・変更をしています。
* 「EPG予約条件」ウィンドウ関係
  * 「自動予約登録を削除」「予約全削除」「前へ」「次へ」ボタン追加。
  * 検索結果一覧の右クリックに「番組名で再検索(サブウィンドウ)」を追加。
* その他
  * 各画面の右クリックメニューを「番組表」をベースにだいたい揃えた。
  * 各設定ウィンドウをESCで閉じられるようにした。
* 設定の説明などを含むコミット 
[f52b17c](https://github.com/tkntrec/EDCB/commit/f52b17cd782a91b6c7da14069f986b428d0f4ddd ) , 
[44dd565](https://github.com/tkntrec/EDCB/commit/44dd565cc3c124f4db456b29343447b6dc11975a ) , 
[afd9850](https://github.com/tkntrec/EDCB/commit/afd985030d4eab085259b5beb44b755deab9fa8a ) , 
[1c22086](https://github.com/tkntrec/EDCB/commit/1c220862bc75b84465d1c524227dbac1c8ee3e3b ) , 
[0ab9449](https://github.com/tkntrec/EDCB/commit/0ab9449b40deea5f6567d45eb7f631c68d0eba96 )

[branch:my-work](https://github.com/tkntrec/EDCB/tree/ct-rb3)
* EpgTimerSrv側の差分です。リベースしたりすることがあります。
* [3ba7902](https://github.com/tkntrec/EDCB/commit/3ba7902797466cd35c4ec1452cb35a52f55bb99b ) 「録画実施後必ずEPG自動録画予約の再チェックが実施されるよう変更」は、他のチューナの録画中にも実行される。元のコードでは録画中に負荷を与えるような処理を極力排除しているようなので、大丈夫とは思うけれど実行環境によっては何か問題が起きる可能性がある。
* [9c86b43](https://github.com/tkntrec/EDCB/commit/9c86b43c5fc8abbf535bdee7859c345d4ca2b648 ) 「追従誤爆を修正」を試しに導入。

[branch:my-build](https://github.com/tkntrec/EDCB/tree/my-build)
* ビルド用の一時ブランチです。