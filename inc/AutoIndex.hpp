/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   AutoIndex.hpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: lmoheyma <lmoheyma@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/04/10 16:29:45 by lmoheyma          #+#    #+#             */
/*   Updated: 2024/04/10 16:29:49 by lmoheyma         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include "main.hpp"

class AutoIndex
{
	public:

	AutoIndex();
	bool isDirectory(const std::string& path);
	bool isFile(const std::string& path);
	std::vector<std::string> listFiles(const std::string& directory);
	std::vector<std::string> listDirectories(const std::string& directory);
	std::string generateAutoIndexHTML(std::string &path);
};