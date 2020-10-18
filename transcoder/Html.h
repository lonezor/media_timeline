#ifndef HTML_H_
#define HTML_H_

#include <string>

using namespace std;

class Html {

public:

	Html(void);

	string page;

	void docTypeHtml(void);
	void beginHtml(void);
	void endHtml(void);

	void beginHeader(void);
	void endHeader(void);

	void beginStyle(void);
	void endStyle(void);

	void addLink(string rel, string href);
	void addScript(string src);
	void beginScript(void);
	void beginScript(string language);
	void endScript(void);

	void beginCssDeclaration(string name);
	void endCssDeclaration(void);
	void addCssEntry(string key, string value);

	void beginBody(void);
	void endBody(void);

	void beginDiv(void);
	void beginDiv(string divClass);
	void beginDiv(string divClass, string tabIndex, string role, string ariaHidden);
	void beginDivImageGallery(string divClass);
	void endDiv(void);

	void beginButton(string id, string btnClass, string onClick);
	void endButton(void);

	void beginTable();
	void beginTable(string style);
	void endTable(void);
	void beginTr(void);
	void endTr(void);
	void beginTd();
	void beginTd(string width);
	void endTd(void);
	void beginB(void);
	void endB(void);
	void beginForm(string action, string method, string enctype);
	void endForm(void);
	void addInput(string type, string name);
	void addInput(string type, string name, string value);
	void addInputMultiple(string type, string name);
	void beginH1();
	void beginH1(string dataTitle);
	void endH1(void);
	void beginH2(void);
	void endH2(void);
	void beginH3(void);
	void endH3(void);
	void beginA(string href);
	void beginA(string href, string aClass);
	void beginAImageGallery(string href);
	void endA(void);

	void addImgImageGallery(string src, string alt);
	void addImg(string src, string alt, string width, string height);
	void addImgOnClick(string onClick, string src);

	void addText(string text);

	void beginUl(void);
	void endUl(void);
	void beginLi(void);
	void endLi(void);

	void beginFigure(void);
	void endFigure(void);
	void beginFigCaption();
	void endFigCaption(void);

	void beginVideo(string id, string width, string height, bool controls);
	void endVideo();
	void addVideoSrc(string errorMsg);
	void addVideoSrc(string src, string type);



};

#endif /* HTML_H_ */
