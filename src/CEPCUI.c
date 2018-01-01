/*******************************************************************************
 * CEPCUI.c: Simple application of file interface using CEP library
 *
 * Copyright (c) 2014, Akihisa Yasuda
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 ******************************************************************************/

#include "m2m/lang/M2MString.h"
#include "m2m/cep/M2MCEP.h"
#include "m2m/db/M2MColumnList.h"
#include "m2m/db/M2MTableManager.h"
#include "m2m/io/M2MHeap.h"
#include "m2m/util/logging/M2MLogger.h"


/*******************************************************************************
 * Declaration of private function
 ******************************************************************************/
/**
 * 規程のディレクトリ配下に入力ファイルが存在するかどうか確認し、ファイルが存在<br>
 * する場合は当該ファイルのデータを読み取り、引数で指定されたポインタにコピー<br>
 * する。<br>
 * なお、入力ファイル名は同一であるため、データのコピーが済み次第、入力ファイル<br>
 * 自体は当該関数が削除する。<br>
 *
 * @param[out] csv	CSV形式の入力データをコピーするためのポインタ(関数内部でヒープメモリを獲得する)
 * @return			コピーした入力データのポインタ or NULL(エラーの場合)
 */
static M2MString *this_getCSV (M2MString **csv);


/**
 * 規程ディレクトリ配下に設置されている入力ファイルのパス文字列を取得する。<br>
 *
 * @param[out] filePath			入力ファイルパス文字列をコピーするためのバッファ
 * @param[in] filePathLength	バッファサイズ[Byte]
 * @return						入力ファイルパス文字列をコピーしたバッファのポインタ or NULL(エラーの場合)
 */
static M2MString *this_getInputFilePath (M2MString filePath[], const size_t filePathLength);


/**
 * 規程ディレクトリ配下の出力ファイルパスを取得する。<br>
 *
 * @param[out] filePath			出力ファイルパス文字列をコピーするためのバッファ
 * @param[in] filePathLength	バッファサイズ[Byte]
 * @return						出力ファイルパス文字列をコピーしたバッファのポインタ or NULL(エラーの場合)
 */
static unsigned char *this_getOutputFilePath (M2MString filePath[], const size_t filePathLength);


/**
 * 規程のディレクトリ配下にCEP処理結果であるCSV形式のファイルを出力する。<br>
 *
 * @param[in] result		CSV形式のCEP処理結果データを示す文字列
 * @param[in] resultLength	CSV形式のCEP処理結果データを示す文字列サイズ[Byte]
 * @return					true : ファイル出力に成功、false : ファイル出力に失敗
 */
static bool this_setResult (const M2MString *result, const size_t resultLength);


/**
 * 引数で指定された時間[usec]だけスリープする。<br>
 *
 * @param[in] time	スリープ時間[usec]
 */
static void this_sleep (unsigned long time);


/**
 * CEP実行の繰り返しを中止するかどうか判定する．
 *
 * @return	true : 中止する，false : 処理を継続する
 */
static bool this_stop ();



/*******************************************************************************
 * Private function
 ******************************************************************************/
/**
 * 入力ファイルの読み込み → CEP → 出力ファイル作成，を繰り返す．
 * 出力ファイルについては，該当する出力が存在しない場合は作成せず，そのままループ<br>
 * 処理を繰り返す．<br>
 *
 * @param[in] cep		CEP実行オブジェクト
 * @@aram[in] tableName	テーブル名を示す文字列
 * @param[in] sql		SELECT文を示す文字列
 * @param[in] sleepTime	CEP繰り返し毎のスリープ時間を示す整数[usec]
 */
static void this_execute (M2MCEP *cep, const M2MString *tableName, const M2MString *sql, const unsigned long sleepTime)
	{
	//========== Variable ==========
	M2MString *csv = NULL;
	M2MString *result = NULL;
	M2MString FILE_PATH[PATH_MAX];
	const M2MString *OUTPUT_FILE_PATH = this_getOutputFilePath(FILE_PATH, sizeof(FILE_PATH));
	const M2MString *METHOD_NAME = (M2MString *)"CEPCUI.this_execute()";

	//===== Check argument =====
	if (cep!=NULL
			&& tableName!=NULL && M2MString_length(tableName)>0
			&& sql!=NULL && M2MString_length(sql)>0)
		{
#ifdef DEBUG
		M2MLogger_printDebugMessage(METHOD_NAME, __LINE__, (M2MString *)"一定間隔でCEPを繰り返すループ処理を開始します");
#endif // DEBUG
		//===== 無限ループ =====
		while (this_stop()==false)
			{
			//===== 出力ファイルが規程ディレクトリ内に存在しなかった場合 =====
			if (M2MFile_exists(OUTPUT_FILE_PATH)==false)
				{
#ifdef DEBUG
				M2MLogger_printDebugMessage(METHOD_NAME, __LINE__, (M2MString *)"規程のディレクトリに設置された出力ファイルが存在しない事を確認しました．．．CEPを実行します");
#endif // DEBUG
				//===== CSV形式のレコードを取得した場合 =====
				if (this_getCSV(&csv)!=NULL)
					{
#ifdef DEBUG
					M2MLogger_printDebugMessage(METHOD_NAME, __LINE__, (M2MString *)"規程のディレクトリに設置されたファイルからCSV形式の入力データを取得しました");
#endif // DEBUG
					//===== CEPデータベースへ挿入 =====
					M2MCEP_insertCSV(cep, tableName, csv);
#ifdef DEBUG
					M2MLogger_printDebugMessage(METHOD_NAME, __LINE__, (M2MString *)"CSV形式の入力データをSQLite3データベースに挿入しました");
					M2MLogger_printDebugMessage(METHOD_NAME, __LINE__, (M2MString *)"CEPを実行します");
#endif // DEBUG
					//===== CEP実行 =====
					if (M2MCEP_select(cep, sql, &result)!=NULL)
						{
#ifdef DEBUG
						M2MLogger_printDebugMessage(METHOD_NAME, __LINE__, (M2MString *)"CEP実行結果のCSV形式の文字列を規程ディレクトリのファイルに出力します");
#endif // DEBUG
						//===== CEP実行結果を出力 =====
						this_setResult(result, M2MString_length(result));
						//===== メモリ領域の解放 =====
						M2MHeap_free(result);
						}
					//===== CEPで条件に合致するデータが存在しなかった場合 =====
					else
						{
#ifdef DEBUG
						M2MLogger_printDebugMessage(METHOD_NAME, __LINE__, (M2MString *)"CEPで合致するレコードが見つかりませんでした");
#endif // DEBUG
						}
					//===== メモリ領域の解放 =====
					M2MHeap_free(csv);
					}
				//===== CSV形式のレコードを取得しなかった場合 =====
				else if (csv==NULL)
					{
#ifdef DEBUG
					M2MLogger_printDebugMessage(METHOD_NAME, __LINE__, (M2MString *)"規程のディレクトリに設置された入力ファイルが見つかりませんでした");
#endif // DEBUG
					}
				}
			//===== 出力ファイルが規程ディレクトリ内に存在する場合 =====
			else
				{
#ifdef DEBUG
				M2MLogger_printDebugMessage(METHOD_NAME, __LINE__, (M2MString *)"規程のディレクトリに設置された出力ファイルが存在するためCEPは実行しません");
#endif // DEBUG
				}
			//===== 一定時間スリープ =====
			this_sleep(sleepTime);
#ifdef DEBUG
			M2MLogger_printDebugMessage(METHOD_NAME, __LINE__, (M2MString *)"CEPを繰り返します");
#endif // DEBUG
			}
		}
	//===== Argument error =====
	else if (cep==NULL)
		{
		M2MLogger_printErrorMessage(METHOD_NAME, __LINE__, (M2MString *)"引数で指定されたCEP実行オブジェクトがNULLです", NULL);
		}
	else if (tableName==NULL || M2MString_length(tableName)<=0)
		{
		M2MLogger_printErrorMessage(METHOD_NAME, __LINE__, (M2MString *)"引数で指定されたテーブル名がNULLです", NULL);
		}
	else
		{
		M2MLogger_printErrorMessage(METHOD_NAME, __LINE__, (M2MString *)"引数で指定されたSQLを示す文字列がNULL，または文字列数が0以下です", NULL);
		}
	return;
	}


/**
 * 規程のディレクトリ配下に入力ファイルが存在するかどうか確認し、ファイルが存在<br>
 * する場合は当該ファイルのデータを読み取り、引数で指定されたポインタにコピー<br>
 * する。<br>
 * なお、入力ファイル名は同一であるため、データのコピーが済み次第、入力ファイル<br>
 * 自体は当該関数が削除する。<br>
 * <br>
 * 【CEP実行のための入出力ファイル有無の条件】<br>
 * ・input.csv : ○, output.csv : ○ → CEP実行 : ×<br>
 * ・input.csv : ○, output.csv : × → CEP実行 : ○<br>
 * ・input.csv : ×, output.csv : ○ → CEP実行 : ×<br>
 * ・input.csv : ×, output.csv : × → CEP実行 : ×<br>
 *
 * @param[out] csv	CSV形式の入力データをコピーするためのポインタ(関数内部でヒープメモリを獲得する)
 * @return			コピーした入力データのポインタ or NULL(エラーの場合)
 */
static M2MString *this_getCSV (M2MString **csv)
	{
	//========== Variable ==========
	M2MString *inputData = NULL;
	M2MString FILE_PATH[PATH_MAX];
	FILE *file = NULL;
	M2MString MESSAGE[256];
	const M2MString *INPUT_FILE_PATH = this_getInputFilePath(FILE_PATH, sizeof(FILE_PATH));
	const M2MString *METHOD_NAME = (M2MString *)"CEPCUI.this_getCSV()";

	//===== Check argument =====
	if (csv!=NULL)
		{
		//===== 入力ファイルが存在する場合 =====
		if (M2MFile_exists(INPUT_FILE_PATH)==true)
			{
			//===== 入力ファイルを開く =====
			if ((file=M2MFile_open(INPUT_FILE_PATH, true))!=NULL)
				{
				//===== CSV形式の入力データをファイルから取得 =====
				if (M2MFile_read(file, &inputData)!=NULL)
					{
					//===== ファイルを閉じる =====
					M2MFile_close(file);
					}
				//===== Error handling =====
				else
					{
					memset(MESSAGE, 0, sizeof(MESSAGE));
					snprintf(MESSAGE, sizeof(MESSAGE)-1, (M2MString *)"規程のディレクトリの入力ファイル(=\"%s\")からのデータ読み取りに失敗しました", INPUT_FILE_PATH);
					M2MLogger_printErrorMessage(METHOD_NAME, __LINE__, MESSAGE, NULL);
					//===== ファイルを閉じる =====
					M2MFile_close(file);
					}
				//===== 改行コードを補正 =====
				if (M2MString_convertFromLFToCRLF(inputData, csv)!=NULL)
					{
					//===== 入力データが存在する場合 =====
					if (inputData!=NULL)
						{
						//===== ヒープメモリ領域を解放 =====
						M2MHeap_free(inputData);
						}
					//===== 入力データが存在しない場合 =====
					else
						{
						// 何もしない
						}
					//===== 入力ファイルを削除 =====
					M2MFile_remove(INPUT_FILE_PATH);
					//===== 正常終了 =====
					return (*csv);
					}
				//===== Error handling =====
				else
					{
					//===== 入力データが存在する場合 =====
					if (inputData!=NULL)
						{
						//===== ヒープメモリ領域を解放 =====
						M2MHeap_free(inputData);
						}
					//===== 入力データが存在しない場合 =====
					else
						{
						// 何もしない
						}
					return NULL;
					}
				}
			//===== Error handling =====
			else
				{
#ifdef DEBUG
				memset(MESSAGE, 0, sizeof(MESSAGE));
				snprintf(MESSAGE, sizeof(MESSAGE)-1, (M2MString *)"規程のディレクトリに入力ファイル(=\"%s\")が存在しません", INPUT_FILE_PATH);
				M2MLogger_printDebugMessage(METHOD_NAME, __LINE__, MESSAGE);
#endif // DEBUG
				return NULL;
				}
			}
		//===== 入力ファイルが存在しない場合 =====
		else
			{
#ifdef DEBUG
			M2MLogger_printDebugMessage(METHOD_NAME, __LINE__, (M2MString *)"規程のディレクトリに入力ファイルが存在しません");
#endif // DEBUG
			return NULL;
			}
		}
	//===== Argument error =====
	else
		{
		M2MLogger_printErrorMessage(METHOD_NAME, __LINE__, (M2MString *)"引数で指定されたCSV形式の入力データをコピーするためのポインタがNULLです", NULL);
		return NULL;
		}
	}


/**
 * 規程ディレクトリ配下の入力ファイルパスを取得する。<br>
 *
 * @param[out] filePath			入力ファイルパス文字列をコピーするためのバッファ
 * @param[in] filePathLength	バッファサイズ[Byte]
 * @return						入力ファイルパス文字列をコピーしたバッファのポインタ or NULL(エラーの場合)
 */
static M2MString *this_getInputFilePath (M2MString filePath[], const size_t filePathLength)
	{
	//========== Variable ==========
	const M2MString *HOME_DIRECTORY = M2MDirectory_getHomeDirectoryPath();
	const M2MString *CEP_DIRECTORY = (M2MString *)M2MCEP_DIRECTORY;
	const M2MString *FILE_NAME = (M2MString *)"input.csv";
	const M2MString *METHOD_NAME = (M2MString *)"CEPCUI.this_getInputFilePath()";

	//===== Check argument =====
	if (filePath!=NULL && filePathLength>0)
		{
		//===== 入力ファイルパスを作成 =====
		memset(filePath, 0, filePathLength);
		snprintf(filePath, filePathLength-1, (M2MString *)"%s/%s/%s", HOME_DIRECTORY, CEP_DIRECTORY, FILE_NAME);
		return filePath;
		}
	//===== Argument error =====
	else if (filePath==NULL)
		{
		M2MLogger_printErrorMessage(METHOD_NAME, __LINE__, (M2MString *)"引数で指定された入力ファイルパス文字列をコピーするためのバッファがNULLです", NULL);
		return NULL;
		}
	else
		{
		M2MLogger_printErrorMessage(METHOD_NAME, __LINE__, (M2MString *)"引数で指定された入力ファイルパス文字列をコピーするためのバッファサイズ[Byte]が0以下です", NULL);
		return NULL;
		}
	}


/**
 * 規程ディレクトリ配下の出力ファイルパスを取得する。<br>
 *
 * @param[out] filePath			出力ファイルパス文字列をコピーするためのバッファ
 * @param[in] filePathLength	バッファサイズ[Byte]
 * @return						出力ファイルパス文字列をコピーしたバッファのポインタ or NULL(エラーの場合)
 */
static M2MString *this_getOutputFilePath (M2MString filePath[], const size_t filePathLength)
	{
	//========== Variable ==========
	const M2MString *HOME_DIRECTORY = M2MDirectory_getHomeDirectoryPath();
	const M2MString *CEP_DIRECTORY = (M2MString *)M2MCEP_DIRECTORY;
	const M2MString *FILE_NAME = (M2MString *)"output.csv";
	const M2MString *METHOD_NAME = (M2MString *)"CEPCUI.this_getOutputFilePath()";

	//===== Check argument =====
	if (filePath!=NULL && filePathLength>0)
		{
		//===== 出力ファイルパスの作成 =====
		memset(filePath, 0, filePathLength);
		snprintf(filePath, filePathLength-1, (M2MString *)"%s/%s/%s", HOME_DIRECTORY, CEP_DIRECTORY, FILE_NAME);
		return filePath;
		}
	//===== Argument error =====
	else if (filePath==NULL)
		{
		M2MLogger_printErrorMessage(METHOD_NAME, __LINE__, (M2MString *)"引数で指定された出力ファイルパス文字列をコピーするためのバッファがNULLです", NULL);
		return NULL;
		}
	else
		{
		M2MLogger_printErrorMessage(METHOD_NAME, __LINE__, (M2MString *)"引数で指定された出力ファイルパス文字列をコピーするためのバッファサイズ[Byte]が0以下です", NULL);
		return NULL;
		}
	}


/**
 * 規程のディレクトリ配下にSELECT用SQLを示すファイル(＝"select.sql")が存在<br>
 * するかどうか確認し、ファイルが存在する場合は当該ファイルのデータを読み取り、<br>
 * 引数で指定されたポインタにコピーする。<br>
 *
 * @param[out] sql	SELECT用SQL文字列をコピーするためのポインタ(関数内部でヒープメモリを獲得する)
 * @return			コピーしたSQL文字列のポインタ or NULL(エラーの場合)
 */
static M2MString *this_getSelectSQL (M2MString **sql)
	{
	//========== Variable ==========
	M2MString INPUT_FILE_PATH[PATH_MAX];
	FILE *file = NULL;
	M2MString MESSAGE[256];
	const M2MString *HOME_DIRECTORY = M2MDirectory_getHomeDirectoryPath();
	const M2MString *CEP_DIRECTORY = (M2MString *)M2MCEP_DIRECTORY;
	const M2MString *FILE_NAME = (M2MString *)"select.sql";
	const M2MString *METHOD_NAME = (M2MString *)"CEPCUI.this_getSelectSQL()";

	//===== Check argument =====
	if (sql!=NULL)
		{
		//===== 入力ファイルパスを作成 =====
		memset(INPUT_FILE_PATH, 0, sizeof(INPUT_FILE_PATH));
		snprintf(INPUT_FILE_PATH, sizeof(INPUT_FILE_PATH)-1, (M2MString *)"%s/%s/%s", HOME_DIRECTORY, CEP_DIRECTORY, FILE_NAME);
		//===== 入力ファイルを開く =====
		if (M2MFile_exists(INPUT_FILE_PATH)==true)
			{
			if ((file=M2MFile_open(INPUT_FILE_PATH, true))!=NULL)
				{
				//===== CSV形式の入力データをファイルから取得 =====
				if (M2MFile_read(file, sql)!=NULL)
					{
					//===== ファイルを閉じる =====
					M2MFile_close(file);
					}
				//===== Error handling =====
				else
					{
					memset(MESSAGE, 0, sizeof(MESSAGE));
					snprintf(MESSAGE, sizeof(MESSAGE)-1, (M2MString *)"規程のディレクトリの入力ファイル(=\"%s\")からデータ読み取りに失敗しました", INPUT_FILE_PATH);
					M2MLogger_printErrorMessage(METHOD_NAME, __LINE__, MESSAGE, NULL);
					//===== ファイルを閉じる =====
					M2MFile_close(file);
					}
				//===== 正常終了 =====
				return (*sql);
				}
			//===== Error handling =====
			else
				{
				memset(MESSAGE, 0, sizeof(MESSAGE));
				snprintf(MESSAGE, sizeof(MESSAGE)-1, (M2MString *)"規程のディレクトリの入力ファイル(=\"%s\")のオープン処理に失敗しました", INPUT_FILE_PATH);
				M2MLogger_printErrorMessage(METHOD_NAME, __LINE__, MESSAGE, NULL);
				return NULL;
				}
			}
		else
			{
#ifdef DEBUG
			memset(MESSAGE, 0, sizeof(MESSAGE));
			snprintf(MESSAGE, sizeof(MESSAGE)-1, (M2MString *)"規程のディレクトリの入力ファイル(=\"%s\")が見つかりません", INPUT_FILE_PATH);
			M2MLogger_printDebugMessage(METHOD_NAME, __LINE__, MESSAGE);
#endif // DEBUG
			return NULL;
			}
		}
	//===== Argument error =====
	else
		{
		M2MLogger_printErrorMessage(METHOD_NAME, __LINE__, (M2MString *)"引数で指定されたSELECT用SQL文字列をコピーするためのポインタがNULLです", NULL);
		return NULL;
		}
	}


/**
 * 規程のディレクトリ配下にCEP処理結果であるCSV形式のファイルを出力する。<br>
 *
 * @param[in] result		CSV形式のCEP処理結果データを示す文字列
 * @param[in] resultLength	CSV形式のCEP処理結果データを示す文字列サイズ[Byte]
 * @return					true : ファイル出力に成功、false : ファイル出力に失敗
 */
static bool this_setResult (const M2MString *result, const size_t resultLength)
	{
	//========== Variable ==========
	FILE *file = NULL;
	M2MString FILE_PATH[PATH_MAX];
	M2MString MESSAGE[256];
	const M2MString *OUTPUT_FILE_PATH = this_getOutputFilePath(FILE_PATH, sizeof(FILE_PATH));
	const M2MString *METHOD_NAME = (M2MString *)"CEPCUI.this_setResult()";

	//===== Check argument =====
	if (result!=NULL && resultLength)
		{
		//===== 出力ファイルパスの確認 =====
		if (OUTPUT_FILE_PATH!=NULL)
			{
			//===== 出力ファイルを新規に開く =====
			if ((file=M2MFile_open(OUTPUT_FILE_PATH, true))!=NULL)
				{
				//===== 出力ファイルにデータ出力 =====
				M2MFile_write(file, result, resultLength);
				M2MFile_close(file);
				return true;
				}
			//===== Error handling =====
			else
				{
				memset(MESSAGE, 0, sizeof(MESSAGE));
				snprintf(MESSAGE, sizeof(MESSAGE)-1, (M2MString *)"出力ファイル(=\"%s\")のオープンに失敗しました", OUTPUT_FILE_PATH);
				M2MLogger_printErrorMessage(METHOD_NAME, __LINE__, MESSAGE, NULL);
				return false;
				}
			}
		//===== Error handling =====
		else
			{
			M2MLogger_printErrorMessage(METHOD_NAME, __LINE__, (M2MString *)"出力ファイルパスを示す文字列がNULLです", NULL);
			return false;
			}
		}
	//===== Argument error =====
	else if (result==NULL)
		{
		M2MLogger_printErrorMessage(METHOD_NAME, __LINE__, (M2MString *)"引数で指定された結果を示すCSV形式の文字列がNULLです", NULL);
		return false;
		}
	else
		{
		M2MLogger_printErrorMessage(METHOD_NAME, __LINE__, (M2MString *)"引数で指定された結果を示すCSV形式の文字列数が0以下です", NULL);
		return false;
		}
	}


/**
 * 引数で指定された時間[usec]だけスリープする。<br>
 *
 * @param[in] time	スリープ時間[usec]
 */
static void this_sleep (unsigned long time)
	{
	//========== Variable ==========
#ifdef DEBUG
	M2MString MESSAGE[128];
	const M2MString *METHOD_NAME = (M2MString *)"CEPCUI.this_sleep()";
#endif // DEBUG
	const unsigned long DEFAULT_SLEEP_TIME = 15000000;

	//===== Check argument =====
	if (time>0)
		{
		// 何もしない
		}
	//===== Argument error =====
	else
		{
		//===== スリープ時間をデフォルト値にセット =====
		time = DEFAULT_SLEEP_TIME;
		}
#ifdef DEBUG
	memset(MESSAGE, 0, sizeof(MESSAGE));
	snprintf(MESSAGE, sizeof(MESSAGE)-1, (M2MString *)"\"%lu\"[usec]の間スリープします", time);
	M2MLogger_printDebugMessage(METHOD_NAME, __LINE__, MESSAGE);
#endif // DEBUG
	//===== スリープ =====
	usleep(time);
	return;
	}


/**
 * CEP実行のループ処理を中止するかどうか判定する．<br>
 * ホームディレクトリの下のcepフォルダ配下に "cepcui.stop" ファイルが存在する<br>
 * 場合，即座にループ処理を中止する（ファイルの中身は空でよい)．<br>
 * 当該ファイルが存在しない場合，そのまま処理を継続する．<br>
 *
 * @return	true : 中止する，false : 処理を継続する
 */
static bool this_stop ()
	{
	//========== Variable ==========
	M2MString FILE_PATH[256];
#ifdef DEBUG
	M2MString MESSAGE[256];
	const M2MString *METHOD_NAME = (M2MString *)"CEPCUI.this_stop()";
#endif // DEBUG
	const M2MString *HOME_DIRECTORY = M2MDirectory_getHomeDirectoryPath();
	const M2MString *CEP_DIRECTORY = (M2MString *)M2MCEP_DIRECTORY;
	const M2MString *FILE_NAME = (M2MString *)"cepcui.stop";

	//===== ファイルパスの作成 =====
	memset(FILE_PATH, 0, sizeof(FILE_PATH));
	snprintf(FILE_PATH, sizeof(FILE_PATH)-1, (M2MString *)"%s/%s/%s", HOME_DIRECTORY, CEP_DIRECTORY, FILE_NAME);

	//===== 中止ファイルが存在している場合 =====
	if (M2MFile_exists(FILE_PATH)==true)
		{
#ifdef DEBUG
		memset(MESSAGE, 0, sizeof(MESSAGE));
		snprintf(MESSAGE, sizeof(MESSAGE)-1, (M2MString *)"ループ処理を中止するためのファイル（＝\"%s\")が存在するため，処理を中止します", FILE_PATH);
		M2MLogger_printDebugMessage(METHOD_NAME, __LINE__, MESSAGE);
#endif // DEBUG
		return true;
		}
	//===== 中止ファイルが存在しない場合 =====
	else
		{
#ifdef DEBUG
		memset(MESSAGE, 0, sizeof(MESSAGE));
		snprintf(MESSAGE, sizeof(MESSAGE)-1, (M2MString *)"ループ処理を中止するためのファイル（＝\"%s\")が存在しないため，処理を継続します", FILE_PATH);
		M2MLogger_printDebugMessage(METHOD_NAME, __LINE__, MESSAGE);
#endif // DEBUG
		return false;
		}
	}


/*******************************************************************************
 * Public function
 ******************************************************************************/
/**
 * Entry point for sample application of CEP shared library.<br>
 *<br>
 * [interface]<br>
 * In this sample application, varies the behavior depending on the files <br>
 * set on the folders below.<br>
 *<br>
 * - File input/output folder: ~/.m2m/cep/<br>
 * - Input file: select.sql (SELECT SQL statement file in CEP described in UTF-8)<br>
 * - Input file: input.csv (record file in CSV format written in UTF-8)<br>
 * - Output file: output.csv (CSV result data detected by specified SELECT SQL statement)<br>
 *<br>
 * If the select.sql file doesn't exist, the application ends.<br>
 *<br>
 * [Application operation]<br>
 * In addition, this application repeats the following processing forever <br>
 * at intervals of 15 [sec].<br>
 *<br>
 * Detect input file → store record → execute SELECT → file output<br>
 *<br>
 * In the input file detection processing, if the corresponding file can't <br>
 * be found, it repeats as it is forever.<br>
 * In the file output processing, if the corresponding record can't be <br>
 * found, no file is output.<br>
 *<br>
 * [Supplement]<br>
 * If an error occurs, log file (~/.m2m/m2m.log) is output.<br>
 * This log file is automatically rotated according to the rule size, <br>
 * so manual deletion processing is unnecessary.<br>
 * However, please keep in mind that since log files are always overwritten <br>
 * output, past log files autoregulated will not remain.<br>
 *
 * @param[in] argc	Number of arguments (max 2)
 * @param[in] argv	The sleep time[usec] of the loop processing and the maximum number of accumulated records (default value = 50)
 * @return			0
 */
int main (int argc, char **argv)
	{
	//========== Variable ==========
	long sleepTime = 0;												// Sleep time[usec]
	int maxRecord = 0;												// Maximum number of accumulated records in SQLite3 memory database
	M2MCEP *cep = NULL;												// CEP object
	M2MTableManager *tableManager = NULL;							// Table information object
	M2MColumnList *columnList = NULL;								// Column information object
	M2MString *sql = NULL;											// SELECT SQL string
	const M2MString *TABLE_NAME = (M2MString *)"cep_test";			// Table name
	const M2MString *DATABASE_NAME = (M2MString *)"cep";			// Database file name
	const M2MString *METHOD_NAME = (M2MString *)"CEPCUI.main()";	// Method name

	//===== When one argument is specified =====
	if (argc==1)
		{
		//===== Get sleep time =====
		if ((sleepTime=M2MString_convertFromStringToLong(argv[1], M2MString_length(argv[1])))>0)
			{
			}
		//===== Error handling =====
		else
			{
			sleepTime = 0;
			}
		}
	//===== When two or more arguments are specified =====
	else if (argc>=2)
		{
		//===== Get sleep time =====
		if ((sleepTime=M2MString_convertFromStringToLong(argv[1], M2MString_length(argv[1])))>0)
			{
			}
		//===== Error handling =====
		else
			{
			sleepTime = 0;
			}
		//===== Get the maximum number of accumulated records in SQLite3 memory database =====
		if ((maxRecord=M2MString_convertFromStringToSignedInteger(argv[2], M2MString_length(argv[2])))>0)
			{
			}
		//===== Error handling =====
		else
			{
			maxRecord = 0;
			}
		}
	//===== When an argument is not specified =====
	else
		{
		// do nothing
		}
#ifdef DEBUG
	M2MLogger_printDebugMessage(METHOD_NAME, __LINE__, (M2MString *)"********** Startup the CEP sample program **********");
#endif // DEBUG
	//===== Get SELECT SQL string =====
	if (this_getSelectSQL(&sql)!=NULL)
		{
		//===== Create new CEP database =====
		if ((columnList=M2MColumnList_new())!=NULL
				&& M2MColumnList_add(columnList, (M2MString *)"date", M2M_DATA_TYPE_DATETIME, false, false, false, false)!=NULL
				&& M2MColumnList_add(columnList, (M2MString *)"name", M2M_DATA_TYPE_TEXT, false, false, false, false)!=NULL
				&& M2MColumnList_add(columnList, (M2MString *)"value", M2M_DATA_TYPE_DOUBLE, false, false, false, false)!=NULL
				&& (tableManager=M2MTableManager_new())!=NULL
				&& M2MTableManager_setConfig(tableManager, TABLE_NAME, columnList)!=NULL
				&& (cep=M2MCEP_new(DATABASE_NAME, tableManager))!=NULL)
			{
			//===== When the number of maximum accumulated record is specified =====
			if (maxRecord>0)
				{
				//===== Set the number of maximum accumulated record in memory database =====
				M2MCEP_setMaxRecord(cep, (unsigned int)maxRecord);
				}
			else
				{
				}
			//===== Execute CEP =====
			this_execute(cep, TABLE_NAME, sql, sleepTime);
			//===== Release heap memory for SQL string =====
			M2MHeap_free(sql);
			//===== Release heap memory for CEP object =====
			M2MCEP_delete(&cep);
			}
		//===== Error handling =====
		else
			{
			M2MLogger_printErrorMessage(METHOD_NAME, __LINE__, (M2MString *)"Failed to construct CEP database", NULL);
			//===== Release heap memory for SQL string =====
			M2MHeap_free(sql);
			}
		}
	//===== Error handling =====
	else
		{
		M2MLogger_printErrorMessage(METHOD_NAME, __LINE__, (M2MString *)"Failed to get SQL string for CEP table search", NULL);
		}
#ifdef DEBUG
	M2MLogger_printDebugMessage(METHOD_NAME, __LINE__, (M2MString *)"********** Quit the CEP sample program **********");
#endif // DEBUG
	return 0;
	}




/* End Of File */
