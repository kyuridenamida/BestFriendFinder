# README

### BestFriendFinder ###
* 大学3年生の夏休みに空き時間があったので作りました．
* TwitterAPIを利用し，Twitter上の特定のユーザー間の共通集合を取ったり・和集合を取ったりするツールです．


### 特徴や注意 ###
* 独自の記法で集合演算を記述すると，様々なユーザーのフォロー/フォロワーの集合に対して集合演算を適用することができます．
* 自分のフォロワーのフォロー情報などから，フォローすべきユーザーを提案する追加機能があります．これがツール名の由来です．
* 見知らぬ鍵アカウントのフォロワー情報を自分のフォロワーのフォロー/フォロワーリストから一部復元することができます．
* keys/mykey.txt　に Twitter開発者ページで生成できるコンシューマーキーやトークン情報を記述しないと動作しません．これが非常に面倒くさいです．

### 使用したライブラリなど ###
* ujson: jsonパーサーライブラリ
* twicpps のパーセントエンコーディング関数: URLエスケープのため(http://www.soramimi.jp/twicpps/さん)
* twicpps のwebclient: http通信のため
* OpenSSL: SHA-1ハッシュ計算のため

### 仕様 ###
* 例えば 「main "either(A)*either(B)-either(C)"」というふうに実行します．
	* 「Aさんと何かしらの繋がりがあってかつBさんと何かしらの繋がりがあるユーザーの中で，Cさんと繋がりのない人」の集合を表示できます．
* 追加機能として，「main --specialFollowing A」でAさんがフォローすべきなのにフォローしていないユーザーを出力することができます．
	* Aさんと繋がりのあるユーザーの中で，その人をフォローしている人が多ければ多い程，その人のスコアが高くなるような計算がなされています．
* ただし，special系コマンドを使う際は，繋がりがある全てのユーザーの情報のクロールが必要なので，予めその処理をしておかなければなりません．
	* つながりのあるユーザー数によってはAPI制限があるため長時間のクロールが必要となります．
* 引数なしで実行すると，ヘルプが表示され，詳細な仕様はそこに書かれています．

```
!X / ~X := not X
A+B     := A or B
A-B     := A and !B
A*B     := A and B
A^B     := A xor B
ing(X)    := users followed by X
ed(X)     := users following X
both(X)   := users followed by X and following X
either(X) := users followed by X or following X
@Ex. ~(~both(user1)^both(user2))*either(user3)
[Special Argument]
--specify          : specify an user from information which you have.
--protected/--key  : list only protected account.
--specialFollowing : list people who you should follow but you don't follow now.
--specialFollowed  : list people who you should be followed but you aren't followed by now.
--specialEither    : list people who you should have any connection with but you don't have.
--specialBoth      : list people who you should have <=> connection with but you don't have.
--threshold=X      : set threshold to X on the special listing mode(Default:X=1).
--filter=Y         : limit the scope on the special listing mode.
--reflesh=Y        : reflesh following/followed information.
```

### 動作環境 ###
* MacOSとLinuxで動作確認済み
* ただし，OpenSSLが入っている環境でないと動かない．

### 工夫した点 ###
* 数式ライクに，自然に集合演算が記述できるような記法にするよう心がけました．
* 独自のアルゴリズムで，フォローすべきユーザーを提案するようにしました．
* 学習のために Oauth 認証の部分の仕様を自前で実装しました．
* TwitterAPIにはAPIの呼び出し制限があるので，キャッシュを保存することで，同じユーザーに対して二回以上のフォロー/フォロワーリストの取得を行わないようにしました．

### 改善したい点 ###
* 現在テキストファイルでデータベースを代替しているので，SQLを使いたいと思っています．
* キャッシュの有効期限などが存在しないので，一度取得すると以降取得しなくなり，古い情報が残ってしまいます(ファイルの削除で対処できますが)．

### 構成 ###
* main.cpp : C++で書かれたメインプログラムです．Makefileがあるのでmakeでコンパイルしてください．
* keys/mykeys.txt : 動作させるためには，キーを取得して記述してください．
* followed/ : 各ユーザーのフォロワーリストのキャッシュが保管されています．
* following/ : 各ユーザーのフォローリストのキャッシュが保管されています．
* info/ : ユーザーのスクリーンネームなどのキャッシュが保管されています．
* 他のファイル : 使用したライブラリです．


	
	
	