#include <SFML/Graphics.hpp>
#include <windows.h>
#include <fstream>
#include <iostream>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <thread>
#include <atomic>
#include <sstream>

#include "binaries.h"

int nRegions=8;
std::string Regions[8]={"LAS",            "LAN",      "BR",         "NA",        "EUW",       "EUNE",         "OCE",          "JP"};
std::string IPs[8]={"138.0.12.100","66.151.33.33","8.23.24.4","192.64.170.1","185.40.65.1","95.172.65.100","103.240.227.5","104.160.154.1"};

using namespace std;

typedef struct Config_s {
    IPAddr nodeAddr;
    char adjust;
    char nodeN;
    int posX;
    int posY;
    bool miniState;
} Config;

typedef struct nConfig_s {
    short server;
    IPAddr nodeAddr;
    char adjust;
    char nodeN;
    int posX;
    int posY;
    bool miniState;
} nConfig;

int loadConfig(nConfig& cnf) {
    std::fstream cnfFile;
    cnfFile.open("config.cnf",cnfFile.in|cnfFile.out|cnfFile.binary);
    if(!cnfFile.is_open()) {
        std::ofstream file;
        file.open("config.cnf",file.out|file.binary);
        if(!file.is_open()) return -1;
        cnf.server=0;
        cnf.adjust=0;
        cnf.nodeN=0;
        cnf.nodeAddr=0;
        cnf.posX=sf::VideoMode::getDesktopMode().width/2-100;
        cnf.posY=sf::VideoMode::getDesktopMode().height/2-100;
        cnf.miniState=false;
        file.write((char*)&cnf,sizeof(cnf));
        file.close();
        return 1;
    }
    /// Versions before 1.3 use a cnf file without "server" field. We have to check if the cnf is an old one and addapt it. Bad news, that file didn't have a version file, so we check by size.
    int a = 0;
    while(!cnfFile.eof()) {a++; cnfFile.ignore(); }
    if(a==21) {
        Config tmpCnf;
        cnfFile.seekg(cnfFile.beg);
        cnfFile.read((char*)&tmpCnf,sizeof(tmpCnf));
        cnf.adjust=tmpCnf.adjust;
        cnf.nodeN=tmpCnf.nodeN;
        cnf.miniState=tmpCnf.miniState;
        cnf.nodeAddr=tmpCnf.nodeAddr;
        cnf.posX=tmpCnf.posX;
        cnf.posY=tmpCnf.posY;
        cnf.server=0;

        cnfFile.seekg(cnfFile.beg);
        cnfFile.write((char*)&cnf,sizeof(cnf));
        return 1;
    }
    cnfFile.seekg(cnfFile.beg);
    cnfFile.read((char*)&cnf,sizeof(cnf));
    cnfFile.close();

    // Version of cnf file:
    return 0;
}

nConfig cnf;
std::atomic<bool> cnfBlock(false);
IPAddr Ip;

int saveConfig(const nConfig cnf) {
    while(cnfBlock) Sleep(10);
    cnfBlock=true;
    std::ofstream cnfFile;
    cnfFile.open("config.cnf",cnfFile.out|cnfFile.binary);
    if(!cnfFile.is_open()) return -1;
    cnfFile.write((char*)&cnf,sizeof(cnf));
    cnfFile.close();
    cnfBlock=false;
    return 0;
}

/*/// Thread in charge of making the pings with differents TTL.
void TraceStep(std::atomic<int>& ended, HANDLE icmp, int ttl, LPVOID reply, DWORD replySize) {

    char SendData[32]="Ping testing -Desaroll";
    IPAddr Ip = inet_addr("138.0.12.100");
    ended = IcmpSendEcho(icmp,Ip,SendData,sizeof(SendData),&Options,reply,replySize,1000);
}*/

std::atomic<bool> close(false),reconfigure(false);
std::atomic<bool> tracerting(false),tracerted(false);
/// For displaying a single character in the wait window;
std::atomic<int> r(0),g(0),b(0);
std::atomic<char> Character(0);
/// For displaying a number
std::atomic<int> Number(0);

#define WINWIDTH 100
#define WINHEIGHT 100

/// Overbutton Detection functions.
bool OverButton(int x, int y, unsigned char button) {
    if(button == 1) return (x < 19          && y < 19);
    if(button == 2) return (x < 19          && y > WINHEIGHT-19);
    if(button == 3) return (x > WINWIDTH-19 && y > WINHEIGHT-19);
    if(button == 4) return (x > WINWIDTH-19 && y < 19);
    return 0;
}

unsigned char OverButton(int x, int y) {
    for(unsigned char a=1; a<5; a++) if(OverButton(x,y,a)) return a;
    return 0;
}

/// "GetInsideTheMonitor" function.
sf::Vector2i GetInside(sf::Vector2i proposed,int winWidth, int winHeight) {
    MONITORINFO monitor;
    monitor.cbSize=sizeof(MONITORINFO);
    RECT win;
    win.top = proposed.y;
    win.left = proposed.x;
    win.bottom = proposed.y+winHeight;
    win.right = proposed.x+winWidth;
    GetMonitorInfo(MonitorFromRect(&win,MONITOR_DEFAULTTONEAREST),&monitor);
    if(proposed.x<monitor.rcWork.left) proposed.x=monitor.rcWork.left;
    if(proposed.x>monitor.rcWork.right-winWidth) proposed.x=monitor.rcWork.right-winWidth;
    if(proposed.y<monitor.rcWork.top) proposed.y=monitor.rcWork.top;
    if(proposed.y>monitor.rcWork.bottom-winHeight) proposed.y=monitor.rcWork.bottom-winHeight;
    return proposed;
}

/// Thread in charge of rendering the window, if needed.
DWORD WINAPI waitBucle(LPVOID params) {
    int winWidth,winHeight;
    winWidth=WINWIDTH;
    winHeight=WINHEIGHT;

    sf::Texture loading,loaded;
    //loading.loadFromFile("loading.png");
    loading.loadFromMemory(loading_png_start,loading_png_size);
    //loaded.loadFromFile("loaded.png");
    loaded.loadFromMemory(loaded_png_start,loaded_png_size);
    loading.setSmooth(true);
    loaded.setSmooth(true);
    sf::Sprite sprite(loading);
    sprite.setOrigin(loading.getSize().x/2,loading.getSize().y/2);
    sprite.setPosition(winWidth/2,winHeight/2);

    sf::RectangleShape border;

    border.setSize(sf::Vector2f(winWidth-4,winHeight-4));
    border.setFillColor(sf::Color::Transparent);
    border.setOutlineColor(sf::Color(170,170,170));
    border.setOutlineThickness(1);
    border.setOrigin(border.getSize().x/2,border.getSize().y/2);
    border.setPosition(winWidth/2,winHeight/2);

    sf::Font font;
    //font.loadFromFile("Mada.ttf");
    font.loadFromMemory(Mada_ttf_start,Mada_ttf_size);
    /*bool* close=((wait_params*)params)->close;
    sf::RenderWindow* app=((wait_params*)params)->app;
    sf::RectangleShape* border=((wait_params*)params)->border;
    sf::Sprite* sprite=((wait_params*)params)->sprite;
    sf::Clock* clock=((wait_params*)params)->clock;*/

    /// Close button components
    sf::RectangleShape closeBorder;
    closeBorder.setFillColor(sf::Color(0,0,0,50));
    closeBorder.setOutlineColor(sf::Color(170,170,170));
    closeBorder.setOutlineThickness(1);
    closeBorder.setSize(sf::Vector2f(16,16));
    closeBorder.setOrigin(8,8);
    closeBorder.setPosition(winWidth-10,10);
    sf::Text closeX("X",font,15);
    closeX.setPosition(86,0);
    /// End of close button components.
    /// Mini button components
    sf::RectangleShape miniBorder;
    miniBorder.setFillColor(sf::Color(0,0,0,50));
    miniBorder.setOutlineColor(sf::Color(170,170,170));
    miniBorder.setOutlineThickness(1);
    miniBorder.setSize(sf::Vector2f(16,16));
    miniBorder.setOrigin(8,8);
    miniBorder.setPosition(10,10);
    sf::Text miniL("=",font,15);
    miniL.setPosition(6,0);
    /// End of mini button components.
    /// + button components
    sf::RectangleShape plusBorder;
    plusBorder.setFillColor(sf::Color(0,0,0,50));
    plusBorder.setOutlineColor(sf::Color(170,170,170));
    plusBorder.setOutlineThickness(1);
    plusBorder.setSize(sf::Vector2f(16,16));
    plusBorder.setOrigin(8,8);
    plusBorder.setPosition(winWidth-10,winHeight-10);
    sf::Text plusL("+",font,15);
    plusL.setPosition(86,81);
    /// End of mini button components.
    /// - button components
    sf::RectangleShape lessBorder;
    lessBorder.setFillColor(sf::Color(0,0,0,50));
    lessBorder.setOutlineColor(sf::Color(170,170,170));
    lessBorder.setOutlineThickness(1);
    lessBorder.setSize(sf::Vector2f(16,16));
    lessBorder.setOrigin(8,8);
    lessBorder.setPosition(10,winHeight-10);
    sf::Text lessL("-",font,15);
    lessL.setPosition(7,81);
    /// End of mini button components.
    bool mini=cnf.miniState;
    if(mini) {
        winHeight=30;
        closeBorder.move(0,33);
        closeX.move(0,33);
        miniBorder.move(0,33);
        miniL.move(0,33);
    }
    sf::RenderWindow app(sf::VideoMode(winWidth, winHeight), "Ping Indicator",0);

    /// Languages submenu
    HMENU regMenu = CreatePopupMenu();
    for(int i=0; i<nRegions; i++) {
        if(i==cnf.server) AppendMenu(regMenu,MF_STRING|MF_CHECKED,100+i,Regions[i].c_str());
        else AppendMenu(regMenu,MF_STRING,100+i,Regions[i].c_str());
    }

    /// Right Click Menu
    HMENU menu = CreatePopupMenu();
    #define RECONFIGUREOPT  1
    #define TRACERTOPT      2
    #define TRACERTCOPYOPT  3
    AppendMenu(menu,MF_STRING,RECONFIGUREOPT,"Reconfigurar");
    AppendMenu(menu,MF_STRING,TRACERTOPT,"Preparar Tracert");
    AppendMenu(menu,MF_DISABLED|MF_STRING,TRACERTCOPYOPT,"Copiar Tracert");
    AppendMenu(menu,MF_POPUP,(UINT_PTR)regMenu,"Región");
    AppendMenu(menu,MF_STRING,0,"Cerrar Menu");
    switch((time(0)/(180))%11) {
    case 0:
        AppendMenu(menu,MF_DISABLED|MF_STRING,0,"Para LAS, por Desaroll");
        break;
    case 1:
        AppendMenu(menu,MF_DISABLED|MF_STRING,0,"¡Libre de Virus!");
        break;
    case 2:
        AppendMenu(menu,MF_DISABLED|MF_STRING,0,"¡Libre de Bugs! (JAJA)");
        break;
    case 3:
        AppendMenu(menu,MF_DISABLED|MF_STRING,0,"¡Sin promesas de pastel!");
        break;
    case 4:
        AppendMenu(menu,MF_DISABLED|MF_STRING,0,"¡Libre de Hongos!");
        break;
    case 5:
        AppendMenu(menu,MF_DISABLED|MF_STRING,0,"No se admiten Teemos");
        break;
    case 6:
        AppendMenu(menu,MF_DISABLED|MF_STRING,0,"¡Cerveza en la Piedra!");
        break;
    case 7:
        AppendMenu(menu,MF_DISABLED|MF_STRING,0,"¿Rick usa cañones Gauss?");
        break;
    case 8:
        AppendMenu(menu,MF_DISABLED|MF_STRING,0,"¿Quien te conoce, Zed(riel)?");
        break;
    case 9:
        AppendMenu(menu,MF_DISABLED|MF_STRING,0,"Enano, Vikingo y Cervecero.");
        break;
    case 10:
        AppendMenu(menu,MF_DISABLED|MF_STRING,0,"¿Lag elevado? ¡Yonner Señal!");
        break;
    }
    /// End of Right Click Menu

    sf::View view(app.getView());
    app.setView(view=sf::View(sf::Vector2f(50,50),sf::Vector2f(100,winHeight)));
    sf::Vector2i proposed(cnf.posX,cnf.posY);
    proposed=GetInside(proposed,winWidth,winHeight);
    app.setPosition(proposed);
    cnf.posX=proposed.x;
    cnf.posY=proposed.y;
    saveConfig(cnf);

    SetWindowPos(app.getSystemHandle(),HWND_TOPMOST,0,0,0,0,SWP_NOSIZE|SWP_NOMOVE);
    app.setFramerateLimit(30);


    bool closeVisible=false;
    sf::Text text("",font,30);
    text.setPosition(50,40);
    unsigned char length=0;
    bool adjust=true;

    int adjAlpha=0;
    sf::Text adj("0",font,15);
    if(cnf.adjust>0) adj.setString("+"+std::to_string(cnf.adjust));
    else adj.setString(std::to_string(cnf.adjust));
    adj.setPosition((winWidth-adj.getGlobalBounds().width)/2,winHeight-2*adj.getGlobalBounds().height);
    adj.setColor(sf::Color(255,255,255,adjAlpha));

    /// IMPORTANT
    /// Pressed variable get if the mouse button was "pressed" in any of the important
    /// positions of the window (Buttons and body), so if the release occurs in the same
    /// place, we activate the button, or move the window.
    /// 0 - Body
    /// 1 - Mini button
    /// 2 - '-' button
    /// 3 - '+' button
    /// 4 - Close button.
    bool pressed[5]{false,false,false,false,false};

    sf::Vector2i mousePos(0,0);

    /// This flag is due a Windows' "bug". When you click the body, and move the mouse
    /// outside the window fast enough as no "mouseMoved" event is registered, no futher
    /// "mouseMoved" events will be registered until you reenter the window.
    /// This doesn't happens once a single "mouseMoved" event is registered, so, if the
    /// first case happens, and the user releases the mouse outside the window, I'll
    /// free the pressed[0] variable, with the "mouseLeft" event this registers.
    bool alreadyMoved = false;

    while (app.isOpen() && close==false)
    {
        sf::Event event;
        while (app.pollEvent(event))
        {
            // Close window : exit
            if (event.type == sf::Event::Closed) {
                close=true;
            }
            if (event.type == sf::Event::LostFocus) closeVisible=false;
            if (event.type == sf::Event::MouseEntered || event.type==sf::Event::MouseMoved) closeVisible=true;
            if (event.type == sf::Event::MouseLeft) { closeVisible=false; if(!alreadyMoved) pressed[0]=false; }
            if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button== sf::Mouse::Left) {
                pressed[OverButton(event.mouseButton.x,event.mouseButton.y)]=true;
                mousePos.x=event.mouseButton.x; mousePos.y=event.mouseButton.y;
            }
            if (event.type == sf::Event::MouseButtonReleased && event.mouseButton.button== sf::Mouse::Left) {
                unsigned char button=OverButton(event.mouseButton.x,event.mouseButton.y);
                sf::Vector2i proposed;
                if(pressed[button]) switch(button) {
                    case 0:
                        alreadyMoved=false;
                    break;
                    case 1:
                        mini=!mini;
                        if(mini) {
                            winHeight=30;
                            closeBorder.move(0,33);
                            closeX.move(0,33);
                            miniBorder.move(0,33);
                            miniL.move(0,33);
                        } else {
                            winHeight=100;
                            closeBorder.move(0,-33);
                            closeX.move(0,-33);
                            miniBorder.move(0,-33);
                            miniL.move(0,-33);
                        }
                        cnf.miniState=mini;
                        app.setSize(sf::Vector2u(100,winHeight));
                        app.setView(view=sf::View(sf::Vector2f(50,50),sf::Vector2f(100,winHeight)));
                        proposed=GetInside(app.getPosition(),winWidth,winHeight);
                        app.setPosition(proposed);
                        cnf.posX=proposed.x;
                        cnf.posY=proposed.y;
                    break;
                    case 2:
                        cnf.adjust--;
                        adjAlpha=255;
                        if(cnf.adjust>0) adj.setString("+"+std::to_string(cnf.adjust));
                        else adj.setString(std::to_string(cnf.adjust));
                        adj.setPosition((winWidth-adj.getGlobalBounds().width)/2,winHeight-2*adj.getGlobalBounds().height);
                        adj.setColor(sf::Color(255,255,255,adjAlpha));
                    break;
                    case 3:
                        cnf.adjust++;
                        adjAlpha=255;
                        if(cnf.adjust>0) adj.setString("+"+std::to_string(cnf.adjust));
                        else adj.setString(std::to_string(cnf.adjust));
                        adj.setPosition((winWidth-adj.getGlobalBounds().width)/2,winHeight-2*adj.getGlobalBounds().height);
                        adj.setColor(sf::Color(255,255,255,adjAlpha));
                    break;
                    case 4:
                        close=true;
                    break;
                }
                saveConfig(cnf);
                pressed[0]=pressed[1]=pressed[2]=pressed[3]=pressed[4]=false;
            }
            if (event.type == sf::Event::MouseMoved) {
                if(pressed[0]){
                    alreadyMoved=true;
                    sf::Vector2i proposed=app.getPosition()+sf::Vector2i(event.mouseMove.x,event.mouseMove.y)-mousePos;
                    proposed=GetInside(proposed,winWidth,winHeight);
                    app.setPosition(proposed);
                    cnf.posX=proposed.x;
                    cnf.posY=proposed.y;
                    saveConfig(cnf);
                } else if(unsigned char button=OverButton(event.mouseMove.x,event.mouseMove.y)) {
                    switch(button) {
                        case 1:
                            miniBorder.setFillColor(sf::Color(255,255,255,50));
                            break;
                        case 2:
                            lessBorder.setFillColor(sf::Color(255,255,255,50));
                            break;
                        case 3:
                            plusBorder.setFillColor(sf::Color(255,255,255,50));
                            break;
                        case 4:
                            closeBorder.setFillColor(sf::Color(255,255,255,50));
                            break;
                    }
                } else {
                    miniBorder.setFillColor(sf::Color(0,0,0,50));
                    lessBorder.setFillColor(sf::Color(0,0,0,50));
                    plusBorder.setFillColor(sf::Color(0,0,0,50));
                    closeBorder.setFillColor(sf::Color(0,0,0,50));
                }
            }
            if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button== sf::Mouse::Right) {
                int menuRet=TrackPopupMenu(menu,TPM_RETURNCMD,app.getPosition().x+event.mouseButton.x,app.getPosition().y+event.mouseButton.y,0,app.getSystemHandle(),0);
                if(menuRet==RECONFIGUREOPT) reconfigure=true;
                else if(menuRet==TRACERTOPT) {
                    tracerting=true;
                    ModifyMenu(menu,TRACERTOPT,MF_BYCOMMAND|MF_STRING|MF_DISABLED,TRACERTOPT,"Preparando Tracert...");
                } else if(menuRet==TRACERTCOPYOPT) {
                    ifstream f("tracert.txt");
                    f.seekg(f.beg);
                    std::stringstream buffer;
                    buffer.clear();
                    buffer << f.rdbuf();
                    const char* output = buffer.str().c_str();
                    const size_t len = strlen(output) + 1;
                    HGLOBAL hMem =  GlobalAlloc(GMEM_MOVEABLE, len);
                    memcpy(GlobalLock(hMem), output, len);
                    GlobalUnlock(hMem);
                    OpenClipboard(0);
                    EmptyClipboard();
                    SetClipboardData(CF_TEXT, hMem);
                    CloseClipboard();
                    f.close();
                } else if(menuRet > 100 && menuRet < 120) {
                    ModifyMenu(regMenu,cnf.server+100,MF_BYCOMMAND|MF_STRING|MF_UNCHECKED,cnf.server+100,Regions[cnf.server].c_str());
                    cnf.server=menuRet-100;
                    saveConfig(cnf);
                    ModifyMenu(regMenu,menuRet,MF_BYCOMMAND|MF_STRING|MF_CHECKED,menuRet,Regions[cnf.server].c_str());
                    Ip = inet_addr(IPs[cnf.server].c_str());
                    reconfigure=true;
                }
            }
        }
        if(tracerted) {
            tracerted=false;
            ModifyMenu(menu,TRACERTOPT,MF_BYCOMMAND|MF_STRING,TRACERTOPT,"Preparar Tracert");
            ModifyMenu(menu,TRACERTCOPYOPT,MF_BYCOMMAND|MF_ENABLED|MF_STRING,TRACERTCOPYOPT,"Copiar Tracert");
        }

        // Process events
        sprite.setRotation(clock()/10);
        app.clear(sf::Color(50,50,50));
        // Clear screen
        // Draw the sprite
        app.draw(border);
        app.draw(sprite);
        if(adjAlpha>0) {
            adjAlpha-=6;
            if(adjAlpha<0) adjAlpha=0;
            adj.setColor(sf::Color(255,255,255,adjAlpha));
            app.draw(adj);
        }
        if(Character!=0) {
            /// ' ' is the special character to render Number as a number, not a caracter.
            /// '_' is the special character to render a ping value (Changing the texture if the value surpasses some margins).
            if(Character==' ') {
                text.setString(std::to_string(Number));
                sprite.setTexture(loading);
                sprite.setColor(sf::Color::White);
            } else if(Character=='_') {
                text.setString(std::to_string(Number));
                sprite.setTexture(loaded);
                sprite.setColor(sf::Color(r,g,b));
            } else if(Character=='.') {
                text.setString("...");
            } else text.setString(std::string(1,Character));
            if(length!=text.getString().getSize()) {
                adjust=true;
                length=text.getString().getSize();
            }
            text.setColor(sf::Color(r,g,b));
            if(adjust) text.setOrigin(text.getLocalBounds().width/2,text.getLocalBounds().height/2);
            app.draw(text);
        }
        if(closeVisible) {
            app.draw(closeBorder);
            app.draw(closeX);
            app.draw(miniBorder);
            app.draw(miniL);
            app.draw(plusBorder);
            app.draw(plusL);
            app.draw(lessBorder);
            app.draw(lessL);
        }

        // Update the window
        app.display();
    }
    app.close();
    return 0;
}

IP_OPTION_INFORMATION Options;
char SendData[32]="Ping testing -Desaroll";
DWORD ReplySize = sizeof(ICMP_ECHO_REPLY) + sizeof(SendData);
LPVOID ReplyBuffer = (VOID*) malloc(ReplySize);

typedef struct PingResult_s {
    PingResult_s(int n,IPAddr ip, int time) {
        this->n=n;
        this->ip=ip;
        this->time=time;
    }
    PingResult_s() { n=0; ip=0; time=0; }
    char n;
    IPAddr ip;
    int time;
} PingResult;


PingResult iPingNode(HANDLE icmp, int a) {
    Options.Ttl=a;
    Options.Tos=0;
    Options.Flags=0;
    Options.OptionsSize=0;
    Options.OptionsData=NULL;
    int start=clock();
    int retorno,retry=0;
    do {
        retorno=IcmpSendEcho(icmp,Ip,SendData,sizeof(SendData),&Options,ReplyBuffer,ReplySize,700);
        retry++;
    } while(retorno==0 && retry < 4);
    if(retorno>0) return PingResult_s(a,((icmp_echo_reply*)ReplyBuffer)->Address,clock()-start);
    else return PingResult_s(a,0,1000);
}

int main()
{
    int a=loadConfig(cnf);
    Ip = inet_addr(IPs[cnf.server].c_str());
    // Create the main window
    // Load a sprite to display
    Character='?';
    r=150;g=150;b=0;
	DWORD wait_t_id;
    HANDLE wait_t_h=CreateThread(
            NULL,                   // default security attributes
            0,                      // use default stack size
            waitBucle,       // thread function name
            0,          // argument to thread function
            0,                      // use default creation flags
            &wait_t_id);   // returns the thread identifier

    HANDLE icmp;
    icmp = IcmpCreateFile();
    if(icmp==INVALID_HANDLE_VALUE) { MessageBox(NULL,"Error creando el conector ICMP. Puedes volver a intentar reabriendo el programa. Avisale a Desaroll por el foro si eso no resulta.","Error",MB_OK|MB_ICONERROR); return -1; }
	// Start the game loop


    /*PingResult lastNode=iPingNode(icmp,cnf.nodeN);
    if(lastNode.ip!=cnf.nodeAddr) {
        PingResult currNode;
        Character=' ';
        r=200;g=100;b=0;
        /// The route seems to have changed. Refinding the last node.
        for(a=1; a<31; a++) {
            if(close) break;
            Number=30-a;
            currNode=iPingNode(icmp,a);
            if(currNode.ip!=0) {
                lastNode=currNode;
            }
        }
        cout << inet_ntoa(*(in_addr*)(&lastNode.ip)) << endl;
        cnf.nodeAddr=lastNode.ip;
        cnf.nodeN=lastNode.n;
        saveConfig(cnf);
    }*/
    int prom = 0;
    while(!close) {
        if(!tracerting) Sleep(300);
        PingResult lastNode=iPingNode(icmp,cnf.nodeN);
        if(tracerting || reconfigure || lastNode.ip!=cnf.nodeAddr) {
            /// The route seems to have changed. Refinding the last node.
            PingResult currNode;
            Character=' ';
            r=100;g=100;b=200;

            for(a=1; a<31; a++) {
                if(close) break;
                Number=30-a;
                currNode=iPingNode(icmp,a);
                if(currNode.ip!=0) {
                    lastNode=currNode;
                }
            }
            cnf.nodeAddr=lastNode.ip;
            cnf.nodeN=lastNode.n;
            saveConfig(cnf);
            reconfigure=false;
            prom=0;
        }
        if(tracerting) {
            Character='.';

            STARTUPINFO siStartInfo;
            PROCESS_INFORMATION piProcInfo;
            SECURITY_ATTRIBUTES sa;
            sa.nLength              =   sizeof  (SECURITY_ATTRIBUTES);
            sa.lpSecurityDescriptor =   NULL;
            sa.bInheritHandle       =   TRUE;
            HANDLE hFile = CreateFile("tracert.txt",                // name of the write
                       GENERIC_WRITE,          // open for writing
                       FILE_SHARE_READ | FILE_SHARE_WRITE,                      // do not share
                       &sa,                   // default security
                       CREATE_ALWAYS,          // create new file only
                       FILE_ATTRIBUTE_NORMAL,  // normal file
                       NULL);
            long unsigned int written;
            WriteFile(hFile,"Tracert creado con el medidor de Ping de Desaroll - LAS.\n\r",std::string("Tracert creado con el medidor de Ping de Desaroll.\n\r").size(),&written,0);
            ZeroMemory( &siStartInfo, sizeof(STARTUPINFO) );
            siStartInfo.cb = sizeof(STARTUPINFO);
            siStartInfo.hStdOutput = hFile;
            siStartInfo.hStdError = hFile;
            siStartInfo.hStdInput = NULL;
            siStartInfo.dwFlags = STARTF_USESTDHANDLES;
            ZeroMemory( &piProcInfo, sizeof(PROCESS_INFORMATION) );
            std::string tmp_s="tracert -h "+std::to_string(cnf.nodeN)+" "+IPs[cnf.server];
            std::vector<char> tmp(tmp_s.begin(), tmp_s.end());
            tmp.push_back('\0');

            if(!CreateProcess(NULL,   // No module name (use command line)
                &tmp[0],        // Command line
                NULL,           // Process handle not inheritable
                NULL,           // Thread handle not inheritable
                TRUE,          // Set handle inheritance to FALSE
                CREATE_NO_WINDOW,//CREATE_NO_WINDOW,              // No creation flags
                NULL,           // Use parent's environment block
                NULL,           // Use parent's starting directory
                &siStartInfo,   // Pointer to STARTUPINFO structure
                &piProcInfo)    // Pointer to PROCESS_INFORMATION structure
            )
            {
                /// Handle Error
            }

            // Wait until child process exits.
            WaitForSingleObject( piProcInfo.hProcess, INFINITE );

            // Close process and thread handles.
            CloseHandle( piProcInfo.hProcess );
            CloseHandle( piProcInfo.hThread );
            CloseHandle( hFile );

            tracerting=false;
            tracerted=true;
        }
        Character='_';
        if(prom==0) prom=lastNode.time;
        prom=(prom*9+lastNode.time)/10;
        Number=prom+cnf.adjust;
        if(Number < 50) { r=0;g=124;b=0; }
        else if(Number < 100) { r=124;g=124;b=0; }
        else { r=180;g=0;b=0; }
    }
    return EXIT_SUCCESS;
}
