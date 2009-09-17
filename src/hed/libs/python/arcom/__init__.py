def import_class_from_string(str):
    """ Import the class which name is in the string
    
    import_class_from_string(str)

    'str' is the name of the class in [package.]*module.classname format.
    """
    module = '.'.join(str.split('.')[:-1]) # cut off class name
    cl = __import__(module) # import module
    for name in str.split('.')[1:]: # start from the first package name
        # step to the next package or the module or the class
        cl = getattr(cl, name)
    return cl # return the class

# for XMLNode

def get_attributes(node):
    return dict([(attribute.Name(), str(attribute)) for attribute in [node.Attribute(i) for i in range(node.AttributesSize())]])

def get_child_nodes(node):
    """ Get all the children nodes of an XMLNode
    
    get_child_nodes(node)
    
    node is the XMLNode whose children we need
    """
    # the node.Size() method returns the number of children
    return [node.Child(i) for i in range(node.Size())]

def get_child_values_by_name(node, name):
    return [str(child) for child in get_child_nodes(node) if child.Name() == name]  

# for DataHandle and DataPoint

def datapoint_from_url(url_string, ssl_config = None):
    import arc
    try:
        xml = arc.XMLNode('<ARCConfig/>')
        if ssl_config.has_key('key_file'):
            xml.NewChild('KeyPath').Set(ssl_config['key_path'])
        if ssl_config.has_key('cert_file'):
            xml.NewChild('CertificatePath').Set(ssl_config['cert_file'])
        if ssl_config.has_key('proxy_file'):
            xml.NewChild('ProxyPath').Set(ssl_config['proxy_file'])
        if ssl_config.has_key('ca_file'):
            xml.NewChild('CACertificatePath').Set(ssl_config['ca_file'])
        if ssl_config.has_key('ca_dir'):
            xml.NewChild('CACertificatesDir').Set(ssl_config['ca_dir'])
        user_config = arc.UserConfig(xml)
    except:
        user_config = arc.UserConfig('')
    handle = arc.DataHandle(arc.URL(url_string), user_config)
    handle.thisown = False

    point = handle.__ref__()
    return point

# for the URL class

def parse_url(url):
    import arc
    url = arc.URL(url)
    proto = url.Protocol()
    host = url.Host()
    port = url.Port()
    path = url.Path()
    return proto, host, int(port), path
