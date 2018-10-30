//#include "stdafx.h"      //for Visual Studio: "Project -> Properties" -> "Configuration Properties -> C/C++ -> Precompiled Headers" , then change the "Precompiled Header" setting to "Not Using Precompiled Headers" option.
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <windows.h>
#include <codecvt>
#include <time.h>
#include <sstream>
#include <regex>
using namespace std;

const int MAX_THREAD_COUNT = 8;
vector<wstring>  m_vFilteredWord;
vector<string>  m_vCmd;
CRITICAL_SECTION m_CriticalSection;
int m_nCurrentBadWord;
int m_nTotalBadWord;
int m_nCount;



//---------------------------------------------------------------------------------------------------
void AddCmd()
{
	m_vCmd.push_back("開始過濾");	m_vCmd.push_back("取代字詞");	m_vCmd.push_back("線呈數量");	m_vCmd.push_back("輸出路徑");	m_vCmd.push_back("輸入路徑");	m_vCmd.push_back("字典路徑");	m_vCmd.push_back("搜尋目標");	m_vCmd.push_back("移除目標");	m_vCmd.push_back("新增目標");	m_vCmd.push_back("離開程式");
}
//---------------------------------------------------------------------------------------------------
enum ECommand
{
	E_RUN,							E_REPLACE_WORD,					E_THREAD_COUNT,					E_OUTPUT_PATH,					E_INPUT_PATH,					E_DICTIONARY_PATH,				E_SEARCH,						E_REMOVE,						E_ADD,							E_EXIT,
};
//---------------------------------------------------------------------------------------------------
struct RunArg
{
	wstring wsInput;
	wstring wsBadWord;
	int* ptr_nArrayMark;
};
//---------------------------------------------------------------------------------------------------
string wstring_to_utf8 (const wstring& str)
{
	wstring_convert<codecvt_utf8<wchar_t>> myconv;
	return myconv.to_bytes(str);
}
//---------------------------------------------------------------------------------------------------
static inline void MarkBadWord(const wstring &str, const wstring& from, const wstring& to,int* ptr_nArrayMark) 
{
	size_t start_pos = 0;
	while((start_pos = str.find(from, start_pos)) != string::npos)
	{
		for(int i =0 ;i<from.length();i++)
			ptr_nArrayMark[start_pos+i]=1;
		start_pos += from.length(); 
	}
}
//---------------------------------------------------------------------------------------------------
void Filter(LPVOID args_)
{
	RunArg *args = (RunArg*)args_;
	EnterCriticalSection(&m_CriticalSection);
	while(m_nCurrentBadWord != m_nTotalBadWord)
	{
		m_nCurrentBadWord++;

		m_nCount++;
		if((m_nTotalBadWord-m_nCount)%5000==0)
			printf("Replacing bad words, remaining:%i\n",m_nTotalBadWord-m_nCount);
		wstring wsSingleBadWord = m_vFilteredWord[m_nCurrentBadWord];
		LeaveCriticalSection(&m_CriticalSection);
		MarkBadWord(args->wsInput,wsSingleBadWord,args->wsBadWord,args->ptr_nArrayMark ); 
		EnterCriticalSection(&m_CriticalSection);
	}
	LeaveCriticalSection(&m_CriticalSection);
}
//---------------------------------------------------------------------------------------------------
wstring Read(string filename)
{
	wifstream wif(filename);
	wif.imbue(locale(locale::empty(), new codecvt_utf8<wchar_t>));
	wstringstream wss;
	wss << wif.rdbuf();
	return wss.str();
}
//---------------------------------------------------------------------------------------------------
void ReadBadWordDic(string sDicFile)
{
	m_vFilteredWord.clear();
	const locale empty_locale = locale::empty();
	typedef codecvt_utf8<wchar_t> converter_type;
	const converter_type* converter = new converter_type;
	const locale utf8_locale = locale(empty_locale, converter);
	wifstream stream((sDicFile));
	stream.imbue(utf8_locale);
	wstring line;

	ifstream inFile(sDicFile); 
	int countline = count(istreambuf_iterator<char>(inFile), 
		istreambuf_iterator<char>(), '\n');
	clock_t t1 = clock();		/*///////////////////////////////////////////*/
	static int i;
	i=0;
	while (getline(stream, line))
	{
		i++;

		if((countline-i)%5000==0)
			cout<<"Reading bad word dictionary, remaining:"<<countline-i<<endl;

		if((line.length())>0)
			m_vFilteredWord.push_back(line);
	}
	clock_t t2 = clock();   	/*///////////////////////////////////////////*/  printf("%s:%f\n", "Reading Time: ",(t2-t1)/(double)(CLOCKS_PER_SEC));
}
//---------------------------------------------------------------------------------------------------
string cinRegMatch(string sMatch)
{
	while(true)
	{
		string buffer;
		getline(cin, buffer);
		regex rx(sMatch);
		smatch m;
		string str = buffer;
		if(regex_search(str, m, rx))
		{
			return buffer;
		}
	}
}
//---------------------------------------------------------------------------------------------------
wstring wcinRegMatch(wstring sMatch)
{
	locale::global(locale(""));
	while(true)
	{
		wstring buffer;
		getline(wcin, buffer);
		wregex  rx(sMatch);
		wsmatch m;
		wstring str = buffer;
		if(regex_search(str, m, rx))
		{
			return buffer;
		}
	}
}
//---------------------------------------------------------------------------------------------------
int cinRegMatchWholeNumber(string sMatch)
{
	while(true)
	{
		string buffer;
		getline(cin, buffer);
		regex rx(sMatch);
		smatch m;
		string str = buffer;
		if(regex_search(str, m, rx))
		{
			int nNum;
			istringstream is(buffer);
			is>>nNum;
			return nNum;
		}
	}
}
//---------------------------------------------------------------------------------------------------
string ExePath() 
{
	char buffer[MAX_PATH];
	GetModuleFileNameA( NULL, buffer, MAX_PATH );
	string::size_type pos = string( buffer ).find_last_of( "\\/" );
	return string( buffer ).substr( 0, pos);
}
//---------------------------------------------------------------------------------------------------
int main()
{
	AddCmd();
	if(!InitializeCriticalSectionAndSpinCount(&m_CriticalSection,0x00000400))
		return 0;

	//global para
	clock_t t1, t2;
	wstring wsOutput;
	HANDLE thd[MAX_THREAD_COUNT];
	DWORD tid;
	bool bRun = false;
	int cmd;
	ofstream myfile;

	//default value
	int nThreadCount=4;
	string sRootPath = ExePath()+"\\";
	string sOutputFile =sRootPath+"Output.txt";
	string sInputFile =sRootPath+"Input.txt";
	string sDicFile =sRootPath+"BadWords.txt";
	wstring wsReplace = L"【X】";

	//case para
	wstring wsRemove;
	wstring wsAdd;
	wstring wsSearch;


	while(true)
	{
		locale::global(locale(""));
		cout<<endl<<endl;
		cout<<"取代字詞:"; wcout<<wsReplace<<endl;
		cout<<"線呈數量:"<<nThreadCount<<endl;
		cout<<"輸出路徑:"<<sOutputFile<<endl;
		cout<<"輸入路徑:"<<sInputFile<<endl;
		cout<<"字典路徑:"<<sDicFile<<endl;

		cout<<endl<<endl;

		cout<<"請輸入CMD："<<endl;
		for(int i=0;i<m_vCmd.size();i++)
			cout<<i<<"."<<m_vCmd[i]<<" ";
		cout<<endl;
		cmd = cinRegMatchWholeNumber("^([0-9]*)$");
		cout<<endl<<endl;

		switch(cmd)//cmd
		{
			//---------------------------------------------
		case (int)E_RUN:
			cout<<"開始過濾"<<endl;
			bRun=true;
			goto run;
			//---------------------------------------------
		case (int)E_REPLACE_WORD:
			cout<<"請輸入 "<<m_vCmd[cmd]<<" ："<<endl;
			wsReplace = wcinRegMatch(L"^.+$");
			goto es;
			//---------------------------------------------
		case (int)E_THREAD_COUNT:
			cout<<"請輸入 "<<m_vCmd[cmd]<<" ："<<endl;
			nThreadCount = cinRegMatchWholeNumber("^[1-8]$");
			goto es;
			//---------------------------------------------
		case (int)E_OUTPUT_PATH:
			cout<<"請輸入 "<<m_vCmd[cmd]<<" ："<<endl;
			while(true)
			{
				getline(cin, sOutputFile);
				if (ifstream(sOutputFile))
				{
					goto es;//file already exist
				}
				else
				{
					ofstream file(sOutputFile); //try create file
					if(file)
						goto es; //create ok
				}
			}
			//---------------------------------------------
		case (int)E_INPUT_PATH:
			cout<<"請輸入 "<<m_vCmd[cmd]<<" ："<<endl;
			while(true)
			{
				getline(cin, sInputFile);
				if (ifstream(sInputFile))
					goto es;
			}
			//---------------------------------------------
		case (int)E_DICTIONARY_PATH:
			cout<<"請輸入 "<<m_vCmd[cmd]<<" ："<<endl;
			while(true)
			{
				getline(cin, sDicFile);
				if (ifstream(sDicFile))
				{
					ReadBadWordDic(sDicFile);
					goto es;
				}
			}
			//---------------------------------------------
		case (int)E_SEARCH:
			if(m_vFilteredWord.size()<1)
				ReadBadWordDic(sDicFile);
			cout<<"請輸入 "<<m_vCmd[cmd]<<" ："<<endl;
			wsSearch = wcinRegMatch(L"^.+$");
			for (vector<wstring>::iterator it = m_vFilteredWord.begin(); it != m_vFilteredWord.end(); it++)
			{
				if(*it==wsSearch)
				{
					cout<<"Found"<<endl;
					goto es;
				}
			}
			cout<<"Not found"<<endl;
			goto es;
			//---------------------------------------------
		case (int)E_REMOVE:
			if(m_vFilteredWord.size()<1)
				ReadBadWordDic(sDicFile);
			cout<<"請輸入 "<<m_vCmd[cmd]<<" ："<<endl;
			wsRemove = wcinRegMatch(L"^.+$");
						for (vector<wstring>::iterator it = m_vFilteredWord.begin(); it != m_vFilteredWord.end(); it++)
			{
				if(*it==wsRemove)
				{
					m_vFilteredWord.erase(it);
					
					wstring wsWord = Read(sDicFile);  int start_pos =0;
					wstring wsRemoveTemp;

					
					wsRemoveTemp = L"\n"+ wsRemove+ L"\n";
					if(( start_pos = wsWord.find(wsRemoveTemp, 0)) != string::npos)
					{
						wsWord.replace(start_pos, wsRemoveTemp.length()-1, L"");
						myfile.open (sDicFile);
						myfile << wstring_to_utf8(wsWord);
						myfile.close();
						goto es;
					}

					wsRemoveTemp = L"\n"+ wsRemove;
					if(( start_pos = wsWord.find(wsRemoveTemp, 0)) != string::npos  && start_pos == wsWord.length()-wsRemoveTemp.length())
					{
						wsWord.replace(start_pos, wsRemoveTemp.length(), L"");
						myfile.open (sDicFile);
						myfile << wstring_to_utf8(wsWord);
						myfile.close();
						goto es;
					}
					
					wsRemoveTemp = wsRemove+ L"\n";
					if(( start_pos = wsWord.find_last_of(wsRemoveTemp, 0)) != string::npos  &&  start_pos == 0)
					{
						wsWord.replace(start_pos, wsRemoveTemp.length(), L"");
						myfile.open (sDicFile);
						myfile << wstring_to_utf8(wsWord);
						myfile.close();
						goto es;
					}					
					
				}
			}
			cout<<"Not found"<<endl;
			goto es;
			//---------------------------------------------
		case (int)E_ADD:
			if(m_vFilteredWord.size()<1)
				ReadBadWordDic(sDicFile);
			cout<<"請輸入 "<<m_vCmd[cmd]<<" ："<<endl;
			wsAdd = wcinRegMatch(L"^.+$");
			for (vector<wstring>::iterator it = m_vFilteredWord.begin(); it != m_vFilteredWord.end(); it++)
			{
				if(*it==wsAdd)
				{
					cout<<"Already exist"<<endl;
					goto es;
				}
			}
			m_vFilteredWord.push_back(wsAdd);
			{
				ofstream log(sDicFile, ios_base::app | ios_base::out);
				log <<endl<< wstring_to_utf8(wsAdd);
			}
			goto es;
			//---------------------------------------------
		case (int)E_EXIT:
			return 0;
			//---------------------------------------------
		default:
			goto es;
			//---------------------------------------------
		}
		//---------------------------------------------
		//---------------------------------------------
		//---------------------------------------------
es://escape_route:
run://run:
		if(bRun)
		{
			bRun = false;
			if(m_vFilteredWord.size()<1)
				ReadBadWordDic(sDicFile);
			wstring wsInput = Read(sInputFile);


			cout<<"字典數量:"<<m_vFilteredWord.size()<<endl;
			cout<<"輸入長度:"<<wsInput.size()<<endl;
			t1 = clock();								/*///////////////////////////////////////////*/
			int* ptr_nArrayMark = new int[wsInput.size()]();
			RunArg args0 = {wsInput,wsReplace,ptr_nArrayMark};
			m_nCurrentBadWord=-1;
			m_nCount = 0;
			m_nTotalBadWord = m_vFilteredWord.size()-1;

			for(int i =0;i<nThreadCount;i++)
				thd[i] = CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)Filter,&args0,0,&tid);

			for(int i=0;i<nThreadCount;i++)
				WaitForSingleObject(thd[i], INFINITE);


			//Special run length encode
			int i = wsInput.size();
			vector<int> vRunLengthEncode;
			for (int j = 0; j < i-1; ++j)
			{
				int count = 1;
				while (j!=wsInput.size()-1 &&ptr_nArrayMark[j]==1  && ptr_nArrayMark[j]== ptr_nArrayMark[j+1])
				{
					count++;
					j++;
				}
				if(ptr_nArrayMark[j]==1)
				{
					vRunLengthEncode.push_back(j-count+1);
					vRunLengthEncode.push_back(count);
				}
			}

			//replace
			wsOutput.assign( wsInput );
			for(int i=vRunLengthEncode.size()-2;i>-1;i-=2)
				wsOutput = wsOutput.replace(vRunLengthEncode[i], vRunLengthEncode[i+1], wsReplace);

			t2 = clock(); 								/*///////////////////////////////////////////*/  printf("%s:%f\n", "Replacing time:",(t2-t1)/(double)(CLOCKS_PER_SEC));

			myfile.open (sOutputFile);
			myfile << wstring_to_utf8(wsOutput);
			myfile.close();
			//---------------------------------------------
			//---------------------------------------------
			//---------------------------------------------
		}
	}
	return 0;
}
//---------------------------------------------------------------------------------------------------
