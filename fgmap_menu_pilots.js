/*
 * Pilots menu
 */

function FGMapMenuPilots(fgmapmenu) {
    this.fgmapmenu = fgmapmenu;
    this.fgmap = fgmapmenu.fgmap;
    this.setup();
}


FGMapMenuPilots.prototype.setup = function() {

    var elem = element_create(null, "div");
    elem.style.width = "100%";
    elem.style.height = "100%";
    elem.style.overflow = "hidden";
    elem.style.paddingTop = "4px";

    var list = this.list = element_create(elem, "div");
    list.style.width = "98%";
    list.style.height = "100%";
    list.style.overflow = "auto";
    list.style.margin = "0px auto";
    list.style.padding = "0px";
    element_hide(list);

    var msg = this.msg = element_create(elem, "div");
    msg.className = "fgmap_pilot_tab_msg";
    msg.style.position = "relative";
    msg.style.textAlign = "center";
    msg.style.verticalAlign = "middle";
    msg.style.marginTop = "15px";
    msg.style.width = "100%";
    msg.style.height = "100%";

    /*
    var info = element_create(elem, "div");
    info.style.position = "absolute";
    info.style.width = "30%";
    info.style.height = "100%";
    info.style.left = "70%";
    info.style.top = "0px";
    info.style.borderLeft = "1px dotted #fff";
    info.style.padding = "12px 4px 0px 8px";
    info.style.lineHeight = "150%";
    info.style.overflow = "hidden";
    */


    this.fgmapmenu.tab_add("pilots", "pilots", elem, this);

    this.server_changed_cb(FGMAP_EVENT_SERVER_CHANGED, null);
    this.fgmap.event_callback_add(FGMAP_EVENT_SERVER_CHANGED,
        this.server_changed_cb.bind_event(this), null);

    this.fgmap.event_callback_add(FGMAP_EVENT_PILOTS_POS_UPDATE,
        this.pilots_pos_update_cb.bind_event(this), null);

    this.fgmap.event_callback_add(FGMAP_EVENT_PILOT_PAN,
        this.pilot_pan_cb.bind_event(this), null);
};


FGMapMenuPilots.prototype.update = function() {

    var cnt = 0;

    this.list.innerHTML = "";

    var ul = element_create(this.list, "ul");
    ul.style.width = "90%";
    ul.style.listStyle = "none inside none";
    ul.style.margin = "0px auto";
    ul.style.padding = "0px"

    var base_li = element_create(null, "li");
    base_li.style.width = "48%";
    base_li.style.cssFloat = "left";
    base_li.style.styleFloat = "left";
    base_li.style.verticalAlign = "middle";
    base_li.style.whiteSpace = "nowrap";
    base_li.style.paddingTop = "6px";
    base_li.style.paddingBottom = "6px";

    for(var callsign in this.fgmap.pilots) {

        var p = this.fgmap.pilots[callsign];
        var marker = p.marker;
        var lng = marker.point.x;
        var lat = marker.point.y;

        var li = element_clone(base_li, false);
        element_attach(li, ul);

        var checkbox = element_create(li, "input", "checkbox");
        checkbox.className = "fgmap_menu";
        checkbox.style.cssFloat = "left";
        checkbox.style.styleFloat = "left";
        checkbox.title = "Tick to make this pilot always visible";
        checkbox.name = "follow_checkbox";
        attach_event(checkbox, "click",
            this.pilot_follow_checkbox_cb.bind_event(this, callsign));

        if(this.fgmap.follows.indexOf(callsign) != -1) {
            checkbox.checked = true;
            if(USER_AGENT.is_ie) {
                checkbox.mychecked = true;
            }
        }

        var span;

        span = element_create(li, "span");
        span.className = "fgmap_pilot_callsign";
        span.style.cssFloat = "left";
        span.style.styleFloat = "left";
        span.style.paddingLeft = "4px";
        span.style.textDecoration = "underline";
        span.style.cursor = "pointer";
        span.title = "Click to pan to this pilot";
        span.innerHTML = callsign;
        attach_event(span, "click",
            this.pilot_callsign_mouse_event_cb.bind_event(this, callsign));
        attach_event(span, "mouseover",
            this.pilot_callsign_mouse_event_cb.bind_event(this, callsign));
        attach_event(span, "mouseout",
            this.pilot_callsign_mouse_event_cb.bind_event(this, callsign));

        element_text_append(li, "@");

        span = element_create(li, "span");
        span.className = "fgmap_pilot_tab_server";
        span.innerHTML = p.server_ip;

        element_create(li, "br");
        element_text_append(li, "\u00a0");

        element_text_append(li, "(");

        span = element_create(li, "span");
        span.className = "fgmap_pilot_tab_lng";
        span.innerHTML = lng.toFixed(4);

        element_text_append(li, " , ");

        span = element_create(li, "span");
        span.className = "fgmap_pilot_tab_lat";
        span.innerHTML = lat.toFixed(4);

        element_text_append(li, ")");

        cnt++;
    }
    
    if(cnt == 0) {
        element_hide(this.list);
        element_show(this.msg);
        this.msg.innerHTML = "No pilots";
    } else {
        element_show(this.list);
        element_hide(this.msg);
    }

};


FGMapMenuPilots.prototype.server_changed_cb = function(event, cb_data, name) {
    element_hide(this.list);
    element_show(this.msg);
    this.msg.innerHTML = "Loading pilots...";
};


FGMapMenuPilots.prototype.pilot_callsign_mouse_event_cb = function(e, callsign) {

    if(!callsign)
        return;

    if(!e) e = window.event;

    if(e.type == "click") {
        this.fgmap.pilot_pan(callsign);
    } else if(e.type == "mouseover") {
        target_get(e).className = "fgmap_pilot_callsign_highlight";
    } else if(e.type == "mouseout") {
        target_get(e).className = "fgmap_pilot_callsign";
    }
};


FGMapMenuPilots.prototype.pilot_follow_checkbox_cb = function(e, callsign) {
    if((target_get(e || window.event)).checked) {
        this.fgmap.pilot_follow_add(callsign);
    } else {
        this.fgmap.pilot_follow_remove(callsign);
    }
};


FGMapMenuPilots.prototype.pilots_pos_update_cb = function(event, cb_data) {
    this.update();
};


FGMapMenuPilots.prototype.pilot_follows_checkboxes_clear = function() {

    var elems = this.list.getElementsByTagName("input");

    for(var i = 0; i < elems.length; i++) {
        if(elems[i].type == "checkbox" && elems[i].name == "follow_checkbox") {
            elems[i].checked = false;
        }
    }

    this.fgmap.pilot_follows_clear();
};


FGMapMenuPilots.prototype.pilot_pan_cb = function(event, cb_data, callsign) {
    if(this.fgmap.follows.indexOf(callsign) == -1) {
        this.pilot_follows_checkboxes_clear();
    }
}




